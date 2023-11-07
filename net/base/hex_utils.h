// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HEX_UTILS_H_
#define NET_BASE_HEX_UTILS_H_

#include <string>
#include <string_view>

#include "net/base/net_export.h"

namespace net {

// Use base::HexEncode() for encoding to hex representation.

// Decode a hex representation like "666f6f" to a string like "foo".  Crashes on
// invalid input in debug builds, therefore it must only be used on sanitized
// input (like a constant literal).  If validity of input needs to be checked or
// partial decoding is desired, use base::HexStringToString() instead.
NET_EXPORT_PRIVATE std::string HexDecode(std::string_view hex);

// Return a std::string containing hex and ASCII representations of the binary
// buffer |input|, with offsets at the beginning of each line, in the style of
// hexdump.  Non-printable characters will be shown as '.' in the ASCII output.
// Example output:
// "0x0000:  0090 69bd 5400 000d 610f 0189 0800 4500  ..i.T...a.....E.\n"
// "0x0010:  001c fb98 4000 4001 7e18 d8ef 2301 455d  ....@.@.~...#.E]\n"
// "0x0020:  7fe2 0800 6bcb 0bc6 806e                 ....k....n\n"
NET_EXPORT_PRIVATE std::string HexDump(std::string_view input);

}  // namespace net

#endif  // NET_BASE_HEX_UTILS_H_
