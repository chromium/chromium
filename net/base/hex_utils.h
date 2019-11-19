// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HEX_UTILS_H_
#define NET_BASE_HEX_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// Return a std::string containing hex and ASCII representations of the binary
// buffer |input|, with offsets at the beginning of each line, in the style of
// hexdump.  Non-printable characters will be shown as '.' in the ASCII output.
// Example output:
// "0x0000:  0090 69bd 5400 000d 610f 0189 0800 4500  ..i.T...a.....E.\n"
// "0x0010:  001c fb98 4000 4001 7e18 d8ef 2301 455d  ....@.@.~...#.E]\n"
// "0x0020:  7fe2 0800 6bcb 0bc6 806e                 ....k....n\n"
NET_EXPORT_PRIVATE std::string HexDump(base::StringPiece input);

}  // namespace net

#endif  // NET_BASE_HEX_UTILS_H_
