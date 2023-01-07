// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_INSPECTOR_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_INSPECTOR_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "v8/include/v8-inspector.h"

namespace auction_worklet {

// Extracts UTF-8 bytes from `s` (converting from UTF-16 if needed).
CONTENT_EXPORT std::vector<uint8_t> GetStringBytes(
    const v8_inspector::StringView& s);

// As above, but for StringBuffer.
inline std::vector<uint8_t> GetStringBytes(v8_inspector::StringBuffer* s) {
  return GetStringBytes(s->string());
}

inline v8_inspector::StringView ToStringView(const std::string& str) {
  return v8_inspector::StringView(reinterpret_cast<const uint8_t*>(str.data()),
                                  str.size());
}

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_INSPECTOR_UTIL_H_
