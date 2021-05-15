// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_PREFLIGHT_RESULT_H_
#define SERVICES_NETWORK_CORS_PREFLIGHT_RESULT_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/strong_alias.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace network {

namespace cors {

// Holds CORS-preflight request results, and provides access check methods.
// Each instance can be cached by CORS-preflight cache.
// See https://fetch.spec.whatwg.org/#concept-cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) PreflightResult final {
 public:
  // Represents whether
  // https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name
  // is supported.
  using WithNonWildcardRequestHeadersSupport =
      base::StrongAlias<class WithNonWildcardRequestHeadersSupportTag, bool>;

  static void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Creates a PreflightResult instance from a CORS-preflight result. Returns
  // nullptr and |detected_error| is populated with the failed reason if the
  // passed parameters contain an invalid entry, and the pointer is valid.
  static std::unique_ptr<PreflightResult> Create(
      const mojom::CredentialsMode credentials_mode,
      const absl::optional<std::string>& allow_methods_header,
      const absl::optional<std::string>& allow_headers_header,
      const absl::optional<std::string>& max_age_header,
      absl::optional<mojom::CorsError>* detected_error);
  ~PreflightResult();

  // Checks if the given |method| is allowed by the CORS-preflight response.
  absl::optional<CorsErrorStatus> EnsureAllowedCrossOriginMethod(
      const std::string& method) const;

  // Checks if the given all |headers| are allowed by the CORS-preflight
  // response.
  // This does not reject when the headers contain forbidden headers
  // (https://fetch.spec.whatwg.org/#forbidden-header-name) because they may be
  // added by the user agent. They must be checked separately and rejected for
  // JavaScript-initiated requests.
  absl::optional<CorsErrorStatus> EnsureAllowedCrossOriginHeaders(
      const net::HttpRequestHeaders& headers,
      bool is_revalidating,
      WithNonWildcardRequestHeadersSupport
          with_non_wildcard_request_headers_support) const;

  // Checks if this entry is expired.
  bool IsExpired() const;

  // Checks if the given combination of |credentials_mode|, |method|, and
  // |headers| is allowed by the CORS-preflight response.
  // This also does not reject the forbidden headers as
  // EnsureAllowCrossOriginHeaders does not.
  bool EnsureAllowedRequest(
      mojom::CredentialsMode credentials_mode,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_revalidating,
      WithNonWildcardRequestHeadersSupport
          with_non_wildcard_request_headers_support) const;

  // Returns true when `headers` has "authorization" which is covered by the
  // wildcard symbol (and not covered by "authorization") in the preflight
  // result.
  // TODO(crbug.com/1176753): Remove this once the investigation is done.
  bool HasAuthorizationCoveredByWildcard(
      const net::HttpRequestHeaders& headers) const;

  // Refers the cache expiry time.
  base::TimeTicks absolute_expiry_time() const { return absolute_expiry_time_; }

 protected:
  explicit PreflightResult(const mojom::CredentialsMode credentials_mode);

  absl::optional<mojom::CorsError> Parse(
      const absl::optional<std::string>& allow_methods_header,
      const absl::optional<std::string>& allow_headers_header,
      const absl::optional<std::string>& max_age_header);

 private:
  absl::optional<CorsErrorStatus>
  EnsureAllowedCrossOriginHeadersWithAuthorizationCoveredByWildcard(
      const net::HttpRequestHeaders& headers,
      bool is_revalidating) const;

  absl::optional<CorsErrorStatus>
  EnsureAllowedCrossOriginHeadersWithAuthorizationNotCoveredByWildcard(
      const net::HttpRequestHeaders& headers,
      bool is_revalidating) const;

  // Holds an absolute time when the result should be expired in the
  // CORS-preflight cache.
  base::TimeTicks absolute_expiry_time_;

  // Corresponds to the fields of the CORS-preflight cache with the same name in
  // the fetch spec.
  // |headers_| holds strings in lower case for case-insensitive search.
  bool credentials_;
  base::flat_set<std::string> methods_;
  base::flat_set<std::string> headers_;

  DISALLOW_COPY_AND_ASSIGN(PreflightResult);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_PREFLIGHT_RESULT_H_
