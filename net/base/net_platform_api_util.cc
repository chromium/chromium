// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_platform_api_util.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"

namespace net {

void CopyStringAndNulToSpan(std::string_view src, base::span<char> dest) {
  dest.copy_prefix_from(base::span(src));
  dest[src.size()] = '\0';
}

std::string_view SpanMaybeWithNulToStringView(base::span<const char> span) {
  size_t length = base::as_string_view(span).find('\0');
  if (length == std::string_view::npos) {
    length = span.size();
  }
  return base::as_string_view(span.first(length));
}

}  // namespace net
