/*
 * libheif example application "heif".
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of heif, an example application using libheif.
 *
 * heif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * heif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with heif.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "box.h"
#include "libde265/de265.h"

#include <fstream>
#include <iostream>

using namespace heif;


int main(int argc, char** argv)
{
  using heif::BoxHeader;
  using heif::Box;
  using heif::fourcc;

  if (argc != 2) {
    fprintf(stderr, "USAGE: %s <filename>\n", argv[0]);
    return 1;
  }

  std::ifstream istr(argv[1]);

  uint64_t maxSize = std::numeric_limits<uint64_t>::max();
  heif::BitstreamRange range(&istr, maxSize);

  std::shared_ptr<Box_meta> meta_box;

  for (;;) {
    std::shared_ptr<Box> box;
    Error error = Box::read(range, &box);
    if (error != Error::OK || range.error() || range.eof()) {
      break;
    }

    heif::Indent indent;
    std::cout << "\n";
    std::cout << box->dump(indent);

    if (box->get_short_type() == fourcc("meta")) {
      meta_box = std::dynamic_pointer_cast<Box_meta>(box);
    }
  }
  if (!meta_box) {
    fprintf(stderr, "Not a valid HEIF file (no 'meta' box found)\n");
    return 1;
  }

  std::vector<std::vector<uint8_t>> images;
  std::ifstream istr2(argv[1]);
  if (!meta_box->get_images(istr2, &images)) {
    fprintf(stderr, "Not a valid HEIF file (could not get images)\n");
    return 1;
  }

  de265_decoder_context* ctx = de265_new_decoder();
  de265_start_worker_threads(ctx,1);
  for (const auto& item : images) {
    de265_push_data(ctx, item.data(), item.size(), 0, nullptr);
  }
#if LIBDE265_NUMERIC_VERSION >= 0x02000000
  de265_push_end_of_stream(ctx);
#else
  de265_flush_data(ctx);
#endif

  FILE* fh = fopen("out.bin", "wb");
  for (const auto& item : images) {
    fwrite(item.data(),1,item.size(),fh);
  }
  fclose(fh);

  for (;;) {
#if LIBDE265_NUMERIC_VERSION >= 0x02000000
    int action = de265_get_action(ctx, 1);
    printf("libde265 action: %d\n",action);

    if (action==de265_action_get_image) {
      printf("image decoded !\n");
    }
#else
    int more;
    de265_error err;
    do {
      more = 0;
      err = de265_decode(ctx, &more);
      if (err != DE265_OK) {
        printf("Error decoding: %s (%d)\n", de265_get_error_text(err), err);
        break;
      }

      const struct de265_image* image = de265_get_next_picture(ctx);
      if (image) {
        printf("Decoded image: %d/%d\n", de265_get_image_width(image, 0),
            de265_get_image_height(image, 0));
        de265_release_next_picture(ctx);
      }
    } while (more);
#endif
    break;
    //#define de265_action_push_more_input     1
    //#define de265_action_end_of_stream       4
  }
}
