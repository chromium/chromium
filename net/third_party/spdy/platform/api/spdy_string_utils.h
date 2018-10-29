// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_STRING_UTILS_H_
#define NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_STRING_UTILS_H_

#include <utility>

// The following header file has to be included from at least
// non-test file in order to avoid strange linking errors.
// TODO(bnc): Remove this include as soon as it is included elsewhere in
// non-test code.
#include "net/third_party/spdy/platform/api/spdy_mem_slice.h"

#include "net/third_party/spdy/platform/api/spdy_string.h"
#include "net/third_party/spdy/platform/api/spdy_string_piece.h"
#include "net/third_party/spdy/platform/impl/spdy_string_utils_impl.h"

namespace spdy {

template <typename... Args>
inline SpdyString SpdyStrCat(const Args&... args) {
  return SpdyStrCatImpl(std::forward<const Args&>(args)...);
}

template <typename... Args>
inline void SpdyStrAppend(SpdyString* output, const Args&... args) {
  SpdyStrAppendImpl(output, std::forward<const Args&>(args)...);
}

inline char SpdyHexDigitToInt(char c) {
  return SpdyHexDigitToIntImpl(c);
}

inline SpdyString SpdyHexDecode(SpdyStringPiece data) {
  return SpdyHexDecodeImpl(data);
}

inline bool SpdyHexDecodeToUInt32(SpdyStringPiece data, uint32_t* out) {
  return SpdyHexDecodeToUInt32Impl(data, out);
}

inline SpdyString SpdyHexEncode(const char* bytes, size_t size) {
  return SpdyHexEncodeImpl(bytes, size);
}

inline SpdyString SpdyHexEncodeUInt32AndTrim(uint32_t data) {
  return SpdyHexEncodeUInt32AndTrimImpl(data);
}

inline SpdyString SpdyHexDump(SpdyStringPiece data) {
  return SpdyHexDumpImpl(data);
}

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_STRING_UTILS_H_
