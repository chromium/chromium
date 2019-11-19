// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/hex_utils.h"

#include <algorithm>

#include "base/strings/stringprintf.h"

namespace net {

std::string HexDump(base::StringPiece input) {
  const int kBytesPerLine = 16;  // Maximum bytes dumped per line.
  int offset = 0;
  const char* buf = input.data();
  int bytes_remaining = input.size();
  std::string output;
  const char* p = buf;
  while (bytes_remaining > 0) {
    const int line_bytes = std::min(bytes_remaining, kBytesPerLine);
    base::StringAppendF(&output, "0x%04x:  ", offset);
    for (int i = 0; i < kBytesPerLine; ++i) {
      if (i < line_bytes) {
        base::StringAppendF(&output, "%02x", static_cast<unsigned char>(p[i]));
      } else {
        output += "  ";
      }
      if (i % 2) {
        output += ' ';
      }
    }
    output += ' ';
    for (int i = 0; i < line_bytes; ++i) {
      // Replace non-printable characters and 0x20 (space) with '.'
      output += (p[i] > 0x20 && p[i] < 0x7f) ? p[i] : '.';
    }

    bytes_remaining -= line_bytes;
    offset += line_bytes;
    p += line_bytes;
    output += '\n';
  }
  return output;
}

}  // namespace net
