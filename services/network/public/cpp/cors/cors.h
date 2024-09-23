// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
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
// TODO(crbug.com/40202951): Remove this.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowExternal[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowHeaders[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowMethods[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowOrigin[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlAllowPrivateNetwork[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlMaxAge[];
// TODO(crbug.com/40202951): Remove this.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestExternal[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestHeaders[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestMethod[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kAccessControlRequestPrivateNetwork[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kPrivateNetworkDeviceId[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kPrivateNetworkDeviceName[];

}  // namespace header_names

// These values are used for logging to UMA. Entries should not be renumbered
// and numeric values should never be reused. Please keep in sync with
// "CorsAccessCheckResult" in src/tools/metrics/histograms/enums.xml.
enum class AccessCheckResult {
  kPermitted = 0,
  kNotPermitted = 1,
  kPermittedInPreflight = 2,
  kNotPermittedInPreflight = 3,

  kMaxValue = kNotPermittedInPreflight,
};

// Performs a CORS access check on the response parameters.
// This implements https://fetch.spec.whatwg.org/#concept-cors-check
COMPONENT_EXPORT(NETWORK_CPP)
base::expected<void, CorsErrorStatus> CheckAccess(
    const GURL& response_url,
    const std::optional<std::string>& allow_origin_header,
    const std::optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode credentials_mode,
    const url::Origin& origin);

// Performs a CORS access check and reports result and error.
COMPONENT_EXPORT(NETWORK_CPP)
base::expected<void, CorsErrorStatus> CheckAccessAndReportMetrics(
    const GURL& response_url,
    const std::optional<std::string>& allow_origin_header,
    const std::optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode credentials_mode,
    const url::Origin& origin);

// Returns true if |request_mode| is not kNavigate nor kNoCors, and the
// |request_initiator| is not same as the origin of |request_url|. The
// |request_url| is expected to have a http or https scheme as they are only
// schemes that the spec officially supports.
COMPONENT_EXPORT(NETWORK_CPP)
bool ShouldCheckCors(const GURL& request_url,
                     const std::optional<url::Origin>& request_initiator,
                     mojom::RequestMode request_mode);

COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsEnabledRequestMode(mojom::RequestMode mode);

// Checks safelisted request parameters.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsSafelistedMethod(const std::string& method);
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsSafelistedContentType(const std::string& name);
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsSafelistedHeader(const std::string& name, const std::string& value);
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

// https://fetch.spec.whatwg.org/#privileged-no-cors-request-header-name
// The returned list is NOT sorted.
// The returned list consists of lower-cased names.
COMPONENT_EXPORT(NETWORK_CPP)
std::vector<std::string> PrivilegedNoCorsHeaderNames();

// Checks forbidden method in the fetch spec.
// See https://fetch.spec.whatwg.org/#forbidden-method.
COMPONENT_EXPORT(NETWORK_CPP) bool IsForbiddenMethod(const std::string& name);

// Returns true if |type| is a response type which makes a response
// CORS-same-origin. See https://html.spec.whatwg.org/C/#cors-same-origin.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsSameOriginResponseType(mojom::FetchResponseType type);

// Returns true if |type| is a response type which makes a response
// CORS-cross-origin. See https://html.spec.whatwg.org/C/#cors-cross-origin.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsCorsCrossOriginResponseType(mojom::FetchResponseType type);

// Returns true if the credentials flag should be set for the given arguments
// as in https://fetch.spec.whatwg.org/#http-network-or-cache-fetch.
COMPONENT_EXPORT(NETWORK_CPP)
bool CalculateCredentialsFlag(mojom::CredentialsMode credentials_mode,
                              mojom::FetchResponseType response_tainting);

// TODO(toyoshim): Consider finding a more organized way to ensure adopting CORS
// checks against all URLLoaderFactory and URLLoader inheritances.
// Calculates mojom::FetchResponseType for non HTTP/HTTPS schemes those are out
// of web standards. This adopts a simplified step 5 of
// https://fetch.spec.whatwg.org/#main-fetch. |mode| is one of the
// network::ResourceRequest to provide a CORS mode for the request.
// |is_request_considered_same_origin| specifies if the request has a special
// permission to bypass CORS checks.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::FetchResponseType CalculateResponseType(
    mojom::RequestMode mode,
    bool is_request_considered_same_origin);

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_H_
