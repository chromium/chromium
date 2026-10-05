// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_UPDATE_PARAMS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_UPDATE_PARAMS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "net/http/http_request_headers.h"

namespace network {

// Represents modifications to the request headers of a network request,
// typically `network::ResourceRequest::headers` and
// `network::ResourceRequest::cors_exempt_headers`.
//
// Modification semantics:
// 1. First, the headers in `removed_headers` should be removed from both of
//    `headers` and `cors_exempt_headers`.
// 2. Then, `modified_headers` and `modified_cors_exempt_headers` should be
//    added to `headers` and `cors_exempt_headers`, respectively.
struct COMPONENT_EXPORT(NETWORK_CPP_HTTP_REQUEST_HEADERS)
    HttpRequestHeadersUpdateParams final {
  HttpRequestHeadersUpdateParams();
  ~HttpRequestHeadersUpdateParams();

  // Move-only.
  HttpRequestHeadersUpdateParams(HttpRequestHeadersUpdateParams&&);
  HttpRequestHeadersUpdateParams& operator=(HttpRequestHeadersUpdateParams&&);
  HttpRequestHeadersUpdateParams(const HttpRequestHeadersUpdateParams&) =
      delete;
  HttpRequestHeadersUpdateParams& operator=(
      const HttpRequestHeadersUpdateParams&) = delete;

  // Applies this update to the given `headers` and `cors_exempt_headers`.
  void Apply(net::HttpRequestHeaders& headers,
             net::HttpRequestHeaders& cors_exempt_headers) const;
  // Variants of Apply that return an inverse.
  //
  // The inverse is an `HttpRequestHeadersUpdateParams` that reverts the
  // update call. In particular:
  // ```
  // net::HttpRequestHeaders headers = ...;
  // net::HttpRequestHeaders cors_exempt_headers = ...;
  // HttpRequestHeadersUpdateParams inverse =
  //    this->ApplyAndReturnInverse(headers, cors_exempt_headers);
  // inverse.Apply(headers, cors_exempt_headers);
  // ```
  //
  // leaves `headers` and `cors_exempt_headers` as they were before the call to
  // `this->ApplyAndReturnInverse()`, assuming no other modifications to
  // `headers` and `cors_exempt_headers` happened between
  // `ApplyAndReturnInverse()` and the inverse being applied.
  //
  // Note that the original header ordering and casing of the state before
  // `ApplyAndReturnInverse()` may not be preserved when the inverse is applied.
  //
  // If the inverse is not needed, use `Apply()` instead.
  [[nodiscard]] HttpRequestHeadersUpdateParams ApplyAndReturnInverse(
      net::HttpRequestHeaders& headers,
      net::HttpRequestHeaders& cors_exempt_headers) const;

  // Merges `other` into `this`, i.e. merging each of the members.
  // TODO(crbug.com/511306597): This is for migrating the existing behavior and
  // is NOT composition: for given `HttpRequestHeadersUpdateParams` `a` and `b`,
  // `a.MergeFrom(b); a.Apply();` isn't identical to `a.Apply(); b.Apply();`.
  // Consider migrating this to composition.
  void MergeFrom(HttpRequestHeadersUpdateParams other);

  // Updates `this` so that it chains both `this` and `other`, i.e.
  // `MergeFromInChain(other); Apply()` is equivalent to `Apply()` followed by
  // `other.Apply()`.
  // This is the mathematical composition of `this` and `other`.
  void MergeFromInChain(const HttpRequestHeadersUpdateParams& other);

  // Modifies this update so that after calling `Apply()`, `headers` has the
  // header `key` set to `value`.
  // Note: This does not remove the header from `removed_headers` (doing so can
  // cause unintended effects, as `removed_headers` removes the header from both
  // the standard headers and the cors-exempt headers).
  void SetHeader(std::string_view key, std::string_view value);
  // Modifies this update so that after calling `Apply()`, `cors_exempt_headers`
  // has the header `key` set to `value`.
  // Note: This does not remove the header from `removed_headers` (doing so can
  // cause unintended effects, as `removed_headers` removes the header from both
  // the standard headers and the cors-exempt headers).
  void SetCorsExemptHeader(std::string_view key, std::string_view value);
  // Modifies this update so that after calling `Apply()`, the header `key` is
  // removed from both `headers` and `cors_exempt_headers`.
  // Note that this is not always the same as just adding the header to
  // `removed_headers` (because `modified_headers` or
  // `modified_cors_exempt_headers` may be re-adding the header).
  void RemoveHeader(std::string_view key);

  void Clear();

  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders modified_headers;
  net::HttpRequestHeaders modified_cors_exempt_headers;

 private:
  HttpRequestHeadersUpdateParams Inverse(
      const net::HttpRequestHeaders& headers,
      const net::HttpRequestHeaders& cors_exempt_headers) const;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_UPDATE_PARAMS_H_
