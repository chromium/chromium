// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_COMMON_CORS_EXEMPT_HEADERS_H_
#define FUCHSIA_WEB_WEBENGINE_COMMON_CORS_EXEMPT_HEADERS_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "fuchsia_web/webengine/web_engine_export.h"

// Sets the list of HTTP header names which will bypass CORS enforcement when
// injected. Must only be called once during single-threaded process startup
// before any concurrent reader runs.
WEB_ENGINE_EXPORT void SetCorsExemptHeaders(
    const std::vector<std::string>& headers);

// Returns the registered casing of `header_name` if it matches a configured
// CORS-exempt header case-insensitively, or `std::nullopt` if no match is found
// or `SetCorsExemptHeaders()` has not yet been invoked.
WEB_ENGINE_EXPORT std::optional<std::string> GetCanonicalCorsExemptHeaderName(
    std::string_view header_name);

// Returns true if the header with `header_name` may bypass CORS when injected.
// Matching of `header_name` is case insensitive, as Chromium's net internals
// don't normalize the casing of header names.
// May only be called after `SetCorsExemptHeaders()` is invoked.
WEB_ENGINE_EXPORT bool IsHeaderCorsExempt(std::string_view header_name);

// Scoped helper for tests that temporarily replaces the process-global
// CORS-exempt header list and restores the previous state on destruction.
class WEB_ENGINE_EXPORT ScopedCorsExemptHeadersForTesting {
 public:
  explicit ScopedCorsExemptHeadersForTesting(
      const std::vector<std::string>& headers);
  ScopedCorsExemptHeadersForTesting(const ScopedCorsExemptHeadersForTesting&) =
      delete;
  ScopedCorsExemptHeadersForTesting& operator=(
      const ScopedCorsExemptHeadersForTesting&) = delete;
  ~ScopedCorsExemptHeadersForTesting();

 private:
  std::optional<base::flat_map<std::string, std::string>> previous_headers_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_COMMON_CORS_EXEMPT_HEADERS_H_
