// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_HTTP2_HPACK_HUFFMAN_HUFFMAN_SPEC_TABLES_H_
#define NET_THIRD_PARTY_HTTP2_HPACK_HUFFMAN_HUFFMAN_SPEC_TABLES_H_

// Tables describing the Huffman encoding of bytes as specified by RFC7541.

#include <cstdint>

namespace http2 {

struct HuffmanSpecTables {
  // Number of bits in the encoding of each symbol (byte).
  static const uint8_t kCodeLengths[257];

  // The encoding of each symbol, right justified (as printed), which means that
  // the last bit of the encoding is the low-order bit of the uint32.
  static const uint32_t kRightCodes[257];
};

}  // namespace http2

#endif  // NET_THIRD_PARTY_HTTP2_HPACK_HUFFMAN_HUFFMAN_SPEC_TABLES_H_
