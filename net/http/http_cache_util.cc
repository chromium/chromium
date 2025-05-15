// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache_util.h"

#include <array>
#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net::http_cache_util {

namespace {

// If the request includes one of these request headers, then avoid caching
// to avoid getting confused.
struct HeaderNameAndValue {
  std::string_view name;
  std::optional<std::string_view> value;
};

// If the request includes one of these request headers, then avoid caching
// to avoid getting confused.
constexpr auto kPassThroughHeaders = std::to_array(
    {HeaderNameAndValue{"if-unmodified-since",
                        std::nullopt},              // causes unexpected 412s
     HeaderNameAndValue{"if-match", std::nullopt},  // causes unexpected 412s
     HeaderNameAndValue{"if-range", std::nullopt}});

// If the request includes one of these request headers, then avoid reusing
// our cached copy if any.
constexpr auto kForceFetchHeaders =
    std::to_array({HeaderNameAndValue{"cache-control", "no-cache"},
                   HeaderNameAndValue{"pragma", "no-cache"}});

// If the request includes one of these request headers, then force our
// cached copy (if any) to be revalidated before reusing it.
constexpr auto kForceValidateHeaders =
    std::to_array({HeaderNameAndValue{"cache-control", "max-age=0"}});

bool HeaderMatches(const HttpRequestHeaders& headers,
                   base::span<const HeaderNameAndValue> search_headers) {
  for (const auto& search_header : search_headers) {
    std::optional<std::string> header_value =
        headers.GetHeader(search_header.name);
    if (!header_value) {
      continue;
    }

    if (!search_header.value) {
      return true;
    }

    HttpUtil::ValuesIterator v(*header_value, ',');
    while (v.GetNext()) {
      if (base::EqualsCaseInsensitiveASCII(v.value(), *search_header.value)) {
        return true;
      }
    }
  }
  return false;
}

struct ValidationHeaderInfo {
  std::string_view request_header_name;
  std::string_view related_response_header_name;
};

constexpr auto kValidationHeaders = std::to_array<ValidationHeaderInfo>(
    {{"if-modified-since", "last-modified"}, {"if-none-match", "etag"}});

}  // namespace

int GetLoadFlagsForExtraHeaders(const HttpRequestHeaders& extra_headers) {
  // Some headers imply load flags.  The order here is significant.
  //
  //   LOAD_DISABLE_CACHE   : no cache read or write
  //   LOAD_BYPASS_CACHE    : no cache read
  //   LOAD_VALIDATE_CACHE  : no cache read unless validation
  //
  // The former modes trump latter modes, so if we find a matching header we
  // can stop iterating kSpecialHeaders.
  static const struct {
    // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (always points to
    // constexpr tables), so there is no benefit to using a raw_ptr, only cost.
    RAW_PTR_EXCLUSION const base::span<const HeaderNameAndValue> search;
    int load_flag;
  } kSpecialHeaders[] = {
      {kPassThroughHeaders, LOAD_DISABLE_CACHE},
      {kForceFetchHeaders, LOAD_BYPASS_CACHE},
      {kForceValidateHeaders, LOAD_VALIDATE_CACHE},
  };
  for (const auto& special_header : kSpecialHeaders) {
    if (HeaderMatches(extra_headers, special_header.search)) {
      return special_header.load_flag;
    }
  }
  static_assert(LOAD_NORMAL == 0);
  return LOAD_NORMAL;
}

// static
base::expected<std::optional<ValidationHeaders>, std::string_view>
ValidationHeaders::MaybeCreate(const HttpRequestHeaders& extra_headers) {
  static_assert(kNumValidationHeaders == std::size(kValidationHeaders),
                "invalid number of validation headers");
  ValidationHeaderValues values;
  bool validation_header_found = false;
  // Check for conditionalization headers which may correspond with a
  // cache validation request.
  for (size_t i = 0; i < std::size(kValidationHeaders); ++i) {
    const ValidationHeaderInfo& info = kValidationHeaders[i];
    if (std::optional<std::string> validation_value =
            extra_headers.GetHeader(info.request_header_name)) {
      if (validation_value->empty()) {
        return base::unexpected("Empty validation header value found");
      }
      values[i] = std::move(*validation_value);
      validation_header_found = true;
    }
  }
  if (validation_header_found) {
    return ValidationHeaders(std::move(values));
  }
  return std::nullopt;
}

ValidationHeaders::ValidationHeaders(ValidationHeaderValues values)
    : values_(std::move(values)) {}

ValidationHeaders::~ValidationHeaders() = default;

ValidationHeaders::ValidationHeaders(ValidationHeaders&&) = default;
ValidationHeaders& ValidationHeaders::operator=(ValidationHeaders&&) = default;

bool ValidationHeaders::Match(
    const HttpResponseHeaders& response_headers) const {
  for (size_t i = 0; i < std::size(kValidationHeaders); i++) {
    if (values_[i].empty()) {
      continue;
    }

    // Retrieve either the cached response's "etag" or "last-modified" header.
    std::optional<std::string_view> validator =
        response_headers.EnumerateHeader(
            nullptr, kValidationHeaders[i].related_response_header_name);

    if (validator && *validator != values_[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace net::http_cache_util
