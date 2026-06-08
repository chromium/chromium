// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/headers_matcher.h"

#include <algorithm>

#include "base/strings/string_util.h"

namespace network {

namespace {

// Normalize the headers:
// - Sort the headers to ignore differences in headers' ordering.
// - Lower-case the header names, as header names should be case-insensitive.
net::HttpRequestHeaders::HeaderVector GetNormalizedHeaders(
    const net::HttpRequestHeaders& headers) {
  net::HttpRequestHeaders::HeaderVector normalized_headers(
      headers.GetHeaderVector());

  for (auto& header : normalized_headers) {
    header.key = base::ToLowerASCII(header.key);
  }

  auto cmp = [](const net::HttpRequestHeaders::HeaderKeyValuePair& a,
                const net::HttpRequestHeaders::HeaderKeyValuePair& b) {
    return a.key < b.key;
  };
  std::sort(normalized_headers.begin(), normalized_headers.end(), cmp);

  return normalized_headers;
}

bool AreValuesMatching(const std::string& expected_value,
                       const std::string& actual_value,
                       MatchHttpRequestHeadersValueOption value_match_option) {
  switch (value_match_option) {
    case MatchHttpRequestHeadersValueOption::kEquals:
      return expected_value == actual_value;
    case MatchHttpRequestHeadersValueOption::kEqualsCaseInsensitiveASCII:
      return base::EqualsCaseInsensitiveASCII(expected_value, actual_value);
  }
}

}  // namespace

MismatchedHttpRequestHeader::MismatchedHttpRequestHeader(
    std::string lowered_key,
    std::optional<std::string> expected_value,
    std::optional<std::string> actual_value)
    : lowered_key(std::move(lowered_key)),
      expected_value(std::move(expected_value)),
      actual_value(std::move(actual_value)) {}

MismatchedHttpRequestHeader::~MismatchedHttpRequestHeader() = default;

MismatchedHttpRequestHeader::MismatchedHttpRequestHeader(
    const MismatchedHttpRequestHeader& other) = default;
MismatchedHttpRequestHeader& MismatchedHttpRequestHeader::operator=(
    const MismatchedHttpRequestHeader& other) = default;
MismatchedHttpRequestHeader::MismatchedHttpRequestHeader(
    MismatchedHttpRequestHeader&& other) = default;
MismatchedHttpRequestHeader& MismatchedHttpRequestHeader::operator=(
    MismatchedHttpRequestHeader&& other) = default;

bool MismatchedHttpRequestHeader::EqualsForTesting(  // IN-TEST
    const MismatchedHttpRequestHeader& other) const {
  return lowered_key == other.lowered_key &&
         expected_value == other.expected_value &&
         actual_value == other.actual_value;
}

std::vector<MismatchedHttpRequestHeader> MatchHttpRequestHeaders(
    const net::HttpRequestHeaders& expected,
    const net::HttpRequestHeaders& actual,
    MatchHttpRequestHeadersValueOption value_match_option,
    base::FunctionRef<bool(const std::string&)> should_ignore) {
  const net::HttpRequestHeaders::HeaderVector expected_headers =
      GetNormalizedHeaders(expected);
  const net::HttpRequestHeaders::HeaderVector actual_headers =
      GetNormalizedHeaders(actual);

  std::vector<MismatchedHttpRequestHeader> mismatched_headers;

  auto add_mismatch_if_not_ignored =
      [&](std::string lowered_key, std::optional<std::string> expected_value,
          std::optional<std::string> actual_value) {
        if (!should_ignore(lowered_key)) {
          mismatched_headers.emplace_back(std::move(lowered_key),
                                          std::move(expected_value),
                                          std::move(actual_value));
        }
      };

  auto expected_it = expected_headers.begin();
  auto actual_it = actual_headers.begin();

  while (expected_it != expected_headers.end() &&
         actual_it != actual_headers.end()) {
    const auto& expected_key = expected_it->key;
    const auto& expected_value = expected_it->value;
    const auto& actual_key = actual_it->key;
    const auto& actual_value = actual_it->value;

    if (expected_key == actual_key) {
      if (!AreValuesMatching(expected_value, actual_value,
                             value_match_option)) {
        add_mismatch_if_not_ignored(expected_key, expected_value, actual_value);
      }
      ++actual_it;
      ++expected_it;
    } else if (actual_key < expected_key) {
      add_mismatch_if_not_ignored(actual_key, /*expected_value=*/std::nullopt,
                                  actual_value);
      ++actual_it;
    } else {
      add_mismatch_if_not_ignored(expected_key, expected_value,
                                  /*actual_value=*/std::nullopt);
      ++expected_it;
    }
  }

  while (actual_it != actual_headers.end()) {
    add_mismatch_if_not_ignored(actual_it->key, /*expected_value=*/std::nullopt,
                                actual_it->value);
    ++actual_it;
  }

  while (expected_it != expected_headers.end()) {
    add_mismatch_if_not_ignored(expected_it->key, expected_it->value,
                                /*actual_value=*/std::nullopt);
    ++expected_it;
  }

  return mismatched_headers;
}

}  // namespace network
