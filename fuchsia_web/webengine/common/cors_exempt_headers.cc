// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/common/cors_exempt_headers.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace {

using CorsExemptHeadersMap = base::flat_map<std::string, std::string>;

// Maps lowercase header names to their registered casing. `std::nullopt` until
// `SetCorsExemptHeaders()` is called.
std::optional<CorsExemptHeadersMap>& GetCorsExemptHeadersMap() {
  static base::NoDestructor<std::optional<CorsExemptHeadersMap>>
      cors_exempt_headers_map;
  return *cors_exempt_headers_map;
}

}  // namespace

void SetCorsExemptHeaders(const std::vector<std::string>& headers) {
  std::optional<CorsExemptHeadersMap>& cors_exempt_headers =
      GetCorsExemptHeadersMap();
  if (!cors_exempt_headers.has_value()) {
    cors_exempt_headers.emplace();
  }
  for (const std::string& header : headers) {
    cors_exempt_headers->try_emplace(base::ToLowerASCII(header), header);
  }
}

std::optional<std::string> GetCanonicalCorsExemptHeaderName(
    std::string_view header_name) {
  const std::optional<CorsExemptHeadersMap>& cors_exempt_headers_map =
      GetCorsExemptHeadersMap();
  if (!cors_exempt_headers_map.has_value()) {
    return std::nullopt;
  }

  auto it = cors_exempt_headers_map->find(base::ToLowerASCII(header_name));
  if (it == cors_exempt_headers_map->end()) {
    return std::nullopt;
  }
  return it->second;
}

bool IsHeaderCorsExempt(std::string_view header_name) {
  CHECK(GetCorsExemptHeadersMap().has_value());
  return GetCanonicalCorsExemptHeaderName(header_name).has_value();
}

ScopedCorsExemptHeadersForTesting::ScopedCorsExemptHeadersForTesting(
    const std::vector<std::string>& headers) {
  std::optional<CorsExemptHeadersMap>& cors_exempt_headers =
      GetCorsExemptHeadersMap();
  previous_headers_ = std::move(cors_exempt_headers);
  cors_exempt_headers.emplace();
  SetCorsExemptHeaders(headers);
}

ScopedCorsExemptHeadersForTesting::~ScopedCorsExemptHeadersForTesting() {
  GetCorsExemptHeadersMap() = std::move(previous_headers_);
}
