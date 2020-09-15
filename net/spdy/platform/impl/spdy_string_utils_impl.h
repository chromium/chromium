// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_PLATFORM_IMPL_SPDY_STRING_UTILS_IMPL_H_
#define NET_SPDY_PLATFORM_IMPL_SPDY_STRING_UTILS_IMPL_H_

#include <sstream>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/hex_utils.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_text_utils.h"

namespace spdy {

template <typename... Args>
inline std::string SpdyStrCatImpl(const Args&... args) {
  std::ostringstream oss;
  int dummy[] = {1, (oss << args, 0)...};
  static_cast<void>(dummy);
  return oss.str();
}

template <typename... Args>
inline void SpdyStrAppendImpl(std::string* output, Args... args) {
  output->append(SpdyStrCatImpl(args...));
}

inline char SpdyHexDigitToIntImpl(char c) {
  return base::HexDigitToInt(c);
}

inline std::string SpdyHexDecodeImpl(quiche::QuicheStringPiece data) {
  return quiche::QuicheTextUtils::HexDecode(data);
}

NET_EXPORT_PRIVATE bool SpdyHexDecodeToUInt32Impl(
    quiche::QuicheStringPiece data,
    uint32_t* out);

inline std::string SpdyHexEncodeImpl(const char* bytes, size_t size) {
  return quiche::QuicheTextUtils::HexEncode(bytes, size);
}

inline std::string SpdyHexEncodeUInt32AndTrimImpl(uint32_t data) {
  return base::StringPrintf("%x", data);
}

inline std::string SpdyHexDumpImpl(quiche::QuicheStringPiece data) {
  return quiche::QuicheTextUtils::HexDump(data);
}

struct SpdyStringPieceCaseHashImpl {
  size_t operator()(quiche::QuicheStringPiece data) const {
    std::string lower = absl::AsciiStrToLower(data);
    absl::Hash<absl::string_view> hasher;
    return hasher(lower);
  }
};

struct SpdyStringPieceCaseEqImpl {
  bool operator()(quiche::QuicheStringPiece piece1,
                  quiche::QuicheStringPiece piece2) const {
    return absl::EqualsIgnoreCase(piece1, piece2);
  }
};

}  // namespace spdy

#endif  // NET_SPDY_PLATFORM_IMPL_SPDY_STRING_UTILS_IMPL_H_
