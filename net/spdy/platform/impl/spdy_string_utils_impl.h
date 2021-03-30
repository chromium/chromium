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
#include "net/third_party/quiche/src/common/platform/api/quiche_text_utils.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace spdy {

inline char SpdyHexDigitToIntImpl(char c) {
  return base::HexDigitToInt(c);
}

inline std::string SpdyHexDecodeImpl(absl::string_view data) {
  return absl::HexStringToBytes(data);
}

NET_EXPORT_PRIVATE bool SpdyHexDecodeToUInt32Impl(absl::string_view data,
                                                  uint32_t* out);

inline std::string SpdyHexEncodeImpl(const char* bytes, size_t size) {
  return absl::BytesToHexString(absl::string_view(bytes, size));
}

inline std::string SpdyHexDumpImpl(absl::string_view data) {
  return quiche::QuicheTextUtils::HexDump(data);
}

}  // namespace spdy

#endif  // NET_SPDY_PLATFORM_IMPL_SPDY_STRING_UTILS_IMPL_H_
