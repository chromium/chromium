// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEB_UI_LOADING_UTIL_H_
#define CONTENT_COMMON_WEB_UI_LOADING_UTIL_H_

#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"

namespace content {

enum class GetRequestedRangeError {
  kNoRanges,
  kMultipleRanges,
  kParseFailed,
};

// Get the requested byte range in the request headers if present. For
// simplicity, only single byte ranges are considered valid input. If there are
// zero or multiple byte ranges, an error is returned. This is deemed
// sufficient for WebUI content.
CONTENT_EXPORT base::expected<net::HttpByteRange, GetRequestedRangeError>
GetRequestedRange(const net::HttpRequestHeaders& headers);

}  // namespace content

#endif  // CONTENT_COMMON_WEB_UI_LOADING_UTIL_H_
