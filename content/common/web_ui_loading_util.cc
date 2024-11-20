// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_ui_loading_util.h"

#include "base/types/expected.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace content {

base::expected<net::HttpByteRange, GetRequestedRangeError> GetRequestedRange(
    const net::HttpRequestHeaders& headers) {
  std::optional<std::string> range_header =
      headers.GetHeader(net::HttpRequestHeaders::kRange);
  if (!range_header) {
    return base::unexpected(GetRequestedRangeError::kNoRanges);
  }
  std::vector<net::HttpByteRange> ranges;
  if (!net::HttpUtil::ParseRangeHeader(*range_header, &ranges)) {
    return base::unexpected(GetRequestedRangeError::kParseFailed);
  }
  if (ranges.size() > 1u) {
    return base::unexpected(GetRequestedRangeError::kMultipleRanges);
  }
  return ranges[0];
}

}  // namespace content
