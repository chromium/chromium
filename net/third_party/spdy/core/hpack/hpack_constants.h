// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_CONSTANTS_H_
#define NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

#include <vector>

#include "net/third_party/spdy/platform/api/spdy_export.h"

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-08

namespace spdy {

// An HpackPrefix signifies |bits| stored in the top |bit_size| bits
// of an octet.
struct HpackPrefix {
  uint8_t bits;
  size_t bit_size;
};

// Represents a symbol and its Huffman code (stored in most-significant bits).
struct HpackHuffmanSymbol {
  uint32_t code;
  uint8_t length;
  uint16_t id;
};

// An entry in the static table. Must be a POD in order to avoid static
// initializers, i.e. no user-defined constructors or destructors.
struct HpackStaticEntry {
  const char* const name;
  const size_t name_len;
  const char* const value;
  const size_t value_len;
};

class HpackHuffmanTable;
class HpackStaticTable;

// Defined in RFC 7540, 6.5.2.
const uint32_t kDefaultHeaderTableSizeSetting = 4096;

// RFC 7541, 5.2: Flag for a string literal that is stored unmodified (i.e.,
// without Huffman encoding).
const HpackPrefix kStringLiteralIdentityEncoded = {0x0, 1};

// RFC 7541, 5.2: Flag for a Huffman-coded string literal.
const HpackPrefix kStringLiteralHuffmanEncoded = {0x1, 1};

// RFC 7541, 6.1: Opcode for an indexed header field.
const HpackPrefix kIndexedOpcode = {0b1, 1};

// RFC 7541, 6.2.1: Opcode for a literal header field with incremental indexing.
const HpackPrefix kLiteralIncrementalIndexOpcode = {0b01, 2};

// RFC 7541, 6.2.2: Opcode for a literal header field without indexing.
const HpackPrefix kLiteralNoIndexOpcode = {0b0000, 4};

// RFC 7541, 6.2.3: Opcode for a literal header field which is never indexed.
// Currently unused.
// const HpackPrefix kLiteralNeverIndexOpcode = {0b0001, 4};

// RFC 7541, 6.3: Opcode for maximum header table size update. Begins a
// varint-encoded table size with a 5-bit prefix.
const HpackPrefix kHeaderTableSizeUpdateOpcode = {0b001, 3};

// Symbol code table from RFC 7541, "Appendix C. Huffman Code".
SPDY_EXPORT_PRIVATE const std::vector<HpackHuffmanSymbol>&
HpackHuffmanCodeVector();

// Static table from RFC 7541, "Appendix B. Static Table Definition".
SPDY_EXPORT_PRIVATE const std::vector<HpackStaticEntry>&
HpackStaticTableVector();

// Returns a HpackHuffmanTable instance initialized with |kHpackHuffmanCode|.
// The instance is read-only, has static lifetime, and is safe to share amoung
// threads. This function is thread-safe.
SPDY_EXPORT_PRIVATE const HpackHuffmanTable& ObtainHpackHuffmanTable();

// Returns a HpackStaticTable instance initialized with |kHpackStaticTable|.
// The instance is read-only, has static lifetime, and is safe to share amoung
// threads. This function is thread-safe.
SPDY_EXPORT_PRIVATE const HpackStaticTable& ObtainHpackStaticTable();

// Pseudo-headers start with a colon.  (HTTP2 8.1.2.1., HPACK 3.1.)
const char kPseudoHeaderPrefix = ':';

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_CONSTANTS_H_
