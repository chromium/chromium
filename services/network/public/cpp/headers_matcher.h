// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HEADERS_MATCHER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HEADERS_MATCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/function_ref.h"
#include "net/http/http_request_headers.h"

namespace network {

// Represents a non-matching header between two `HttpRequestHeaders` objects,
// say, `expected` and `actual`.
struct COMPONENT_EXPORT(NETWORK_CPP) MismatchedHttpRequestHeader final {
 public:
  MismatchedHttpRequestHeader(std::string lowered_key,
                              std::optional<std::string> expected_value,
                              std::optional<std::string> actual_value);
  ~MismatchedHttpRequestHeader();

  // Copyable and movable.
  MismatchedHttpRequestHeader(const MismatchedHttpRequestHeader& other);
  MismatchedHttpRequestHeader& operator=(
      const MismatchedHttpRequestHeader& other);
  MismatchedHttpRequestHeader(MismatchedHttpRequestHeader&& other);
  MismatchedHttpRequestHeader& operator=(MismatchedHttpRequestHeader&& other);

  bool EqualsForTesting(const MismatchedHttpRequestHeader& other) const;

  // Lower-cased header name.
  std::string lowered_key;

  // Header values of `expected` and `actual`, respectively.
  // `std::nullopt` indicates the corresponding `HttpRequestHeaders` does not
  // have the header.
  std::optional<std::string> expected_value;
  std::optional<std::string> actual_value;
};

// Indicates how to match the header values in `MatchHttpRequestHeaders()`.
enum class MatchHttpRequestHeadersValueOption {
  // Header values should be exactly the same.
  kEquals,

  // Header values are compared using `base::EqualsCaseInsensitiveASCII()`.
  kEqualsCaseInsensitiveASCII,
};

// Compares `expected` and `actual`, and returns headers that didn't match
// between these two.
// `should_ignore()` is called with each lower-cased header name, and when it
// returns `true`, the header is ignored.
// If no mismatch is found, this returns an empty vector.
COMPONENT_EXPORT(NETWORK_CPP)
std::vector<MismatchedHttpRequestHeader> MatchHttpRequestHeaders(
    const net::HttpRequestHeaders& expected,
    const net::HttpRequestHeaders& actual,
    MatchHttpRequestHeadersValueOption value_match_option,
    base::FunctionRef<bool(const std::string& /*lowered_key*/)> should_ignore =
        [](const std::string&) { return false; });

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HEADERS_MATCHER_H_
