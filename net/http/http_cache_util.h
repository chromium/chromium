// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_CACHE_UTIL_H_
#define NET_HTTP_HTTP_CACHE_UTIL_H_

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/types/expected.h"

namespace net {

class HttpRequestHeaders;
class HttpResponseHeaders;

namespace http_cache_util {

// Determines cache-related load flags based on the provided HTTP request
// headers.
//
// This function inspects `extra_headers` for patterns implying specific cache
// behaviors (e.g., "Cache-Control: no-cache", "If-Match"). It can return
// flags like LOAD_DISABLE_CACHE, LOAD_BYPASS_CACHE, or LOAD_VALIDATE_CACHE.
//
// Returns an int representing the determined load flags , or 0 (LOAD_NORMAL) if
// no special cache-related headers are found.
int GetLoadFlagsForExtraHeaders(const HttpRequestHeaders& extra_headers);

// Manages HTTP validation headers (e.g., If-Modified-Since, If-None-Match)
// provided in a request. It can parse them from HttpRequestHeaders and
// match them against HttpResponseHeaders.
class ValidationHeaders {
 public:
  // Attempts to create a ValidationHeaders object by parsing
  // "If-Modified-Since" and "If-None-Match" headers from the provided
  // `extra_headers`. Returns an ValidationHeaders if one or more valid
  // validation headers are found. Returns std::nullopt if no relevant headers
  // are present. Returns base::unexpected on error (e.g., an empty header
  // value).
  static base::expected<std::optional<ValidationHeaders>, std::string_view>
  MaybeCreate(const HttpRequestHeaders& extra_headers);

  ~ValidationHeaders();

  ValidationHeaders(const ValidationHeaders&) = delete;
  ValidationHeaders& operator=(const ValidationHeaders&) = delete;

  ValidationHeaders(ValidationHeaders&&);
  ValidationHeaders& operator=(ValidationHeaders&&);

  // Checks if the provided `response_headers` satisfy the validation
  // conditions. This compares stored "If-Modified-Since" with "Last-Modified"
  // and "If-None-Match" with "ETag" from the `response_headers`.
  bool Match(const HttpResponseHeaders& response_headers) const;

 private:
  static const size_t kNumValidationHeaders = 2;
  using ValidationHeaderValues = std::array<std::string, kNumValidationHeaders>;

  explicit ValidationHeaders(ValidationHeaderValues values);

  ValidationHeaderValues values_;
};

}  // namespace http_cache_util
}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_UTIL_H_
