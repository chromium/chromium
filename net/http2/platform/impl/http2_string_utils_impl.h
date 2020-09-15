// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP2_PLATFORM_IMPL_HTTP2_STRING_UTILS_IMPL_H_
#define NET_HTTP2_PLATFORM_IMPL_HTTP2_STRING_UTILS_IMPL_H_

#include <sstream>
#include <utility>

#include "base/strings/abseil_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/escape.h"
#include "net/base/hex_utils.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_text_utils.h"

namespace http2 {

template <typename... Args>
inline std::string Http2StrCatImpl(const Args&... args) {
  std::ostringstream oss;
  int dummy[] = {1, (oss << args, 0)...};
  static_cast<void>(dummy);
  return oss.str();
}

template <typename... Args>
inline void Http2StrAppendImpl(std::string* output, Args... args) {
  output->append(Http2StrCatImpl(args...));
}

template <typename... Args>
inline std::string Http2StringPrintfImpl(const Args&... args) {
  return base::StringPrintf(std::forward<const Args&>(args)...);
}

inline std::string Http2HexEncodeImpl(const void* bytes, size_t size) {
  return base::HexEncode(bytes, size);
}

inline std::string Http2HexDecodeImpl(quiche::QuicheStringPiece data) {
  return quiche::QuicheTextUtils::HexDecode(data);
}

inline std::string Http2HexDumpImpl(quiche::QuicheStringPiece data) {
  return quiche::QuicheTextUtils::HexDump(data);
}

inline std::string Http2HexEscapeImpl(quiche::QuicheStringPiece data) {
  return net::EscapeQueryParamValue(base::StringViewToStringPiece(data), false);
}

template <typename Number>
inline std::string Http2HexImpl(Number number) {
  std::stringstream str;
  str << std::hex << number;
  return str.str();
}

}  // namespace http2

#endif  // NET_HTTP2_PLATFORM_IMPL_HTTP2_STRING_UTILS_IMPL_H_
