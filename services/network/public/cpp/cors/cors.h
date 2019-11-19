// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

class GURL;
namespace url {
class Origin;
}  // namespace url

namespace network {

namespace cors {

namespace header_names {

COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowCredentials[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowExternal[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowHeaders[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowMethods[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowOrigin[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlMaxAge[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestExternal[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestHeaders[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestMethod[];

}  // namespace header_names

// Performs a CORS access check on the response parameters.
// This implements https://fetch.spec.whatwg.org/#concept-cors-check
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<CorsErrorStatus> CheckAccess(
    const GURL& response_url,
    const int response_status_code,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode credentials_mode,
    const url::Origin& origin);

// Returns true if |request_mode| is not kNavigate nor kNoCors, and the
// origin of |request_url| is not a data URL, and |request_initiator| is not
// same as the origin of |request_url|,
COMPONENT_EXPORT(NETWORK_CPP)
bool ShouldCheckCors(const GURL& request_url,
                     const base::Optional<url::Origin>& request_initiator,
                     mojom::RequestMode request_mode);

// Performs a CORS access check on the CORS-preflight response parameters.
// According to the note at https://fetch.spec.whatwg.org/#cors-preflight-fetch
// step 6, even for a preflight check, |credentials_mode| should be checked on
// the actual request rather than preflight one.
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<CorsErrorStatus> CheckPreflightAccess(
    const GURL& response_url,
    const int response_status_code,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode actual_credentials_mode,
    const url::Origin& origin);

// Given a redirected-to URL, checks if the location is allowed
// according to CORS. That is:
// - the URL has a CORS supported scheme and
// - the URL does not contain the userinfo production.
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<CorsErrorStatus> CheckRedirectLocation(
    const GURL& url,
    mojom::RequestMode request_mode,
    const base::Optional<url::Origin>& origin,
    bool cors_flag,
    bool tainted);

// Performs the required CORS checks on the response to a preflight request.
// Returns |kPreflightSuccess| if preflight response was successful.
// TODO(toyoshim): Rename to CheckPreflightStatus.
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<mojom::CorsError> CheckPreflight(const int status_code);

// Checks errors for the currently experimental "Access-Control-Allow-External:"
// header. Shares error conditions with standard preflight checking.
// See https://crbug.com/590714.
COMPONENT_EXPORT(NETWORK_CPP)
base::Optional<CorsErrorStatus> CheckExternalPreflight(
    const base::Optional<std::string>& allow_external);

COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsEnabledRequestMode(mojom::RequestMode mode);

// Checks safelisted request parameters.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsSafelistedMethod(const std::string& method);
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsSafelistedContentType(const std::string& name);
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsSafelistedHeader(
    const std::string& name,
    const std::string& value,
    const base::flat_set<std::string>& extra_safelisted_header_names = {});
COMPONENT_EXPORT(NETWORK_CPP)
bool IsNoCorsSafelistedHeaderName(const std::string& name);
COMPONENT_EXPORT(NETWORK_CPP)
bool IsPrivilegedNoCorsHeaderName(const std::string& name);
COMPONENT_EXPORT(NETWORK_CPP)
bool IsNoCorsSafelistedHeader(const std::string& name,
                              const std::string& value);

// https://fetch.spec.whatwg.org/#cors-unsafe-request-header-names
// |headers| must not contain multiple headers for the same name.
// The returned list is NOT sorted.
// The returned list consists of lower-cased names.
COMPONENT_EXPORT(NETWORK_CPP)
std::vector<std::string> CorsUnsafeRequestHeaderNames(
    const net::HttpRequestHeaders::HeaderVector& headers);

// https://fetch.spec.whatwg.org/#cors-unsafe-request-header-names
// Returns header names which are not CORS-safelisted AND not forbidden.
// |headers| must not contain multiple headers for the same name.
// When |is_revalidating| is true, "if-modified-since", "if-none-match", and
// "cache-control" are also exempted.
// The returned list is NOT sorted.
// The returned list consists of lower-cased names.
COMPONENT_EXPORT(NETWORK_CPP)
std::vector<std::string> CorsUnsafeNotForbiddenRequestHeaderNames(
    const net::HttpRequestHeaders::HeaderVector& headers,
    bool is_revalidating,
    const base::flat_set<std::string>& extra_safelisted_header_names = {});

// Checks forbidden method in the fetch spec.
// See https://fetch.spec.whatwg.org/#forbidden-method.
// TODO(toyoshim): Move Blink FetchUtils::IsForbiddenMethod to cors:: and use
// this implementation internally.
COMPONENT_EXPORT(NETWORK_CPP) bool IsForbiddenMethod(const std::string& name);

// https://fetch.spec.whatwg.org/#ok-status aka a successful 2xx status code,
// https://tools.ietf.org/html/rfc7231#section-6.3 . We opt to use the Fetch
// term in naming the predicate.
COMPONENT_EXPORT(NETWORK_CPP) bool IsOkStatus(int status);

// Returns true if |type| is a response type which makes a response
// CORS-same-origin. See https://html.spec.whatwg.org/#cors-same-origin.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsSameOriginResponseType(mojom::FetchResponseType type);

// Returns true if |type| is a response type which makes a response
// CORS-cross-origin. See https://html.spec.whatwg.org/#cors-cross-origin.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsCrossOriginResponseType(mojom::FetchResponseType type);

// Returns true if the credentials flag should be set for the given arguments
// as in https://fetch.spec.whatwg.org/#http-network-or-cache-fetch.
COMPONENT_EXPORT(NETWORK_CPP)
bool CalculateCredentialsFlag(mojom::CredentialsMode credentials_mode,
                              mojom::FetchResponseType response_tainting);

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_
