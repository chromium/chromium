// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/fourccs.h"

#include <sstream>
#include <string>

namespace media::mp4 {

std::string FourCCToString(FourCC fourcc) {
  char buf[5];
  buf[0] = (fourcc >> 24) & 0xff;
  buf[1] = (fourcc >> 16) & 0xff;
  buf[2] = (fourcc >> 8) & 0xff;
  buf[3] = (fourcc)&0xff;
  buf[4] = 0;

  // Return hex itself if characters can not be printed. Any character within
  // the "C" locale is considered printable.
  for (int i = 0; i < 4; ++i) {
    if (!(buf[i] > 0x1f && buf[i] < 0x7f)) {
      std::stringstream hex_string;
      hex_string << "0x" << std::hex << fourcc;
      return hex_string.str();
    }
  }

  return std::string(buf);
}

}  // namespace media::mp4
