// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/http_request_headers_update_params.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/strings/string_util.h"

namespace network {

HttpRequestHeadersUpdateParams::HttpRequestHeadersUpdateParams() = default;
HttpRequestHeadersUpdateParams::~HttpRequestHeadersUpdateParams() = default;
HttpRequestHeadersUpdateParams::HttpRequestHeadersUpdateParams(
    HttpRequestHeadersUpdateParams&&) = default;
HttpRequestHeadersUpdateParams& HttpRequestHeadersUpdateParams::operator=(
    HttpRequestHeadersUpdateParams&&) = default;

void HttpRequestHeadersUpdateParams::Apply(
    net::HttpRequestHeaders& headers,
    net::HttpRequestHeaders& cors_exempt_headers) const {
  for (const auto& removed_header : removed_headers) {
    headers.RemoveHeader(removed_header);
    cors_exempt_headers.RemoveHeader(removed_header);
  }
  headers.MergeFrom(modified_headers);
  cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);
}

HttpRequestHeadersUpdateParams
HttpRequestHeadersUpdateParams::ApplyAndReturnInverse(
    net::HttpRequestHeaders& headers,
    net::HttpRequestHeaders& cors_exempt_headers) const {
  HttpRequestHeadersUpdateParams inverse =
      Inverse(headers, cors_exempt_headers);
  Apply(headers, cors_exempt_headers);

  return inverse;
}

void HttpRequestHeadersUpdateParams::MergeFrom(
    HttpRequestHeadersUpdateParams other) {
  for (std::string& removed_header : other.removed_headers) {
    // TODO(crbug.com/511306597): To turn this into composition, we have to do:
    // ```
    // modified_headers.RemoveHeader(removed_header);
    // modified_cors_exempt_headers.RemoveHeader(removed_header);
    // ```
    if (!std::ranges::contains(removed_headers, removed_header)) {
      removed_headers.emplace_back(std::move(removed_header));
    }
  }
  modified_headers.MergeFrom(other.modified_headers);
  modified_cors_exempt_headers.MergeFrom(other.modified_cors_exempt_headers);
}

void HttpRequestHeadersUpdateParams::MergeFromInChain(
    const HttpRequestHeadersUpdateParams& other) {
  // The code below would break if we get `*this` as `other`.
  // `a.MergeFromInChain(a)` is a no-op, so handle that case early.
  if (this == &other) {
    return;
  }
  for (const std::string& removed_header : other.removed_headers) {
    RemoveHeader(removed_header);
  }
  modified_headers.MergeFrom(other.modified_headers);
  modified_cors_exempt_headers.MergeFrom(other.modified_cors_exempt_headers);
}

HttpRequestHeadersUpdateParams HttpRequestHeadersUpdateParams::Inverse(
    const net::HttpRequestHeaders& headers,
    const net::HttpRequestHeaders& cors_exempt_headers) const {
  HttpRequestHeadersUpdateParams result;

  // For every key touched by `this`, restore both `headers` and
  // `cors_exempt_headers` to their pre-update state.
  //
  // `restore()` is idempotent per key, and `RemoveHeader()` de-duplicates
  // case-insensitively.
  auto restore = [&](std::string_view key) {
    std::optional<std::string> old_value = headers.GetHeader(key);
    std::optional<std::string> old_cors_exempt_value =
        cors_exempt_headers.GetHeader(key);
    if (!old_value.has_value() || !old_cors_exempt_value.has_value()) {
      result.RemoveHeader(key);
    }
    if (old_value.has_value()) {
      result.SetHeader(key, *old_value);
    }
    if (old_cors_exempt_value.has_value()) {
      result.SetCorsExemptHeader(key, *old_cors_exempt_value);
    }
  };

  for (const std::string& key : removed_headers) {
    restore(key);
  }

  for (const auto& header : modified_headers.GetHeaderVector()) {
    restore(header.key);
  }

  for (const auto& header : modified_cors_exempt_headers.GetHeaderVector()) {
    restore(header.key);
  }

  return result;
}

void HttpRequestHeadersUpdateParams::SetHeader(std::string_view key,
                                               std::string_view value) {
  modified_headers.SetHeader(key, value);
}

void HttpRequestHeadersUpdateParams::SetCorsExemptHeader(
    std::string_view key,
    std::string_view value) {
  modified_cors_exempt_headers.SetHeader(key, value);
}

void HttpRequestHeadersUpdateParams::RemoveHeader(std::string_view key) {
  modified_headers.RemoveHeader(key);
  modified_cors_exempt_headers.RemoveHeader(key);
  if (!std::ranges::any_of(removed_headers, [&](const std::string& header) {
        return base::EqualsCaseInsensitiveASCII(header, key);
      })) {
    removed_headers.emplace_back(key);
  }
}

void HttpRequestHeadersUpdateParams::Clear() {
  removed_headers.clear();
  modified_headers.Clear();
  modified_cors_exempt_headers.Clear();
}

}  // namespace network
