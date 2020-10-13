// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/platform/impl/spdy_flags_impl.h"

// If true, use http2::HuffmanSize() instead of
// spdy::HpackHuffmanTable::EncodedSize() and http2::HuffmanEncodeFast()
// instead of spdy::HpackHuffmanTable::EncodeString() for HPACK encoding used
// in HTTP/2 and Google QUIC (not IETF QUIC).
bool http2_use_fast_huffman_encoder = true;

namespace spdy {}  // namespace spdy
