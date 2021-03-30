// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/mime_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/request_mode.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

// String conversion from blink::String to std::string for header name/value
// should be latin-1, not utf-8, as per HTTP. Note that as we use ByteString
// as the IDL types of header name/value, a character whose code point is
// greater than 255 has already been blocked.

namespace network {

namespace {

const char kAsterisk[] = "*";
const char kLowerCaseTrue[] = "true";

// TODO(toyoshim): Consider to move the following method to
// //net/base/mime_util, and expose to Blink platform/network in order to
// replace the existing equivalent method in HTTPParser.
// We may prefer to implement a strict RFC2616 media-type
// (https://tools.ietf.org/html/rfc2616#section-3.7) parser.
std::string ExtractMIMETypeFromMediaType(const std::string& media_type) {
  std::string::size_type semicolon = media_type.find(';');
  std::string top_level_type;
  std::string subtype;
  if (net::ParseMimeTypeWithoutParameter(media_type.substr(0, semicolon),
                                         &top_level_type, &subtype)) {
    return top_level_type + "/" + subtype;
  }
  return std::string();
}

// Returns true only if |header_value| satisfies ABNF: 1*DIGIT [ "." 1*DIGIT ]
bool IsSimilarToDoubleABNF(const std::string& header_value) {
  if (header_value.empty())
    return false;
  char first_char = header_value.at(0);
  if (!isdigit(first_char))
    return false;

  bool period_found = false;
  bool digit_found_after_period = false;
  for (char ch : header_value) {
    if (isdigit(ch)) {
      if (period_found) {
        digit_found_after_period = true;
      }
      continue;
    }
    if (ch == '.') {
      if (period_found)
        return false;
      period_found = true;
      continue;
    }
    return false;
  }
  if (period_found)
    return digit_found_after_period;
  return true;
}

// Returns true only if |header_value| satisfies ABNF: 1*DIGIT
bool IsSimilarToIntABNF(const std::string& header_value) {
  if (header_value.empty())
    return false;

  for (char ch : header_value) {
    if (!isdigit(ch))
      return false;
  }
  return true;
}

// https://fetch.spec.whatwg.org/#cors-unsafe-request-header-byte
bool IsCorsUnsafeRequestHeaderByte(char c) {
  const auto u = static_cast<uint8_t>(c);
  return (u < 0x20 && u != 0x09) || u == 0x22 || u == 0x28 || u == 0x29 ||
         u == 0x3a || u == 0x3c || u == 0x3e || u == 0x3f || u == 0x40 ||
         u == 0x5b || u == 0x5c || u == 0x5d || u == 0x7b || u == 0x7d ||
         u == 0x7f;
}

// |value| should be lower case.
bool IsCorsSafelistedLowerCaseContentType(const std::string& value) {
  DCHECK_EQ(value, base::ToLowerASCII(value));
  if (std::any_of(value.begin(), value.end(), IsCorsUnsafeRequestHeaderByte))
    return false;

  std::string mime_type = ExtractMIMETypeFromMediaType(value);
  return mime_type == "application/x-www-form-urlencoded" ||
         mime_type == "multipart/form-data" || mime_type == "text/plain";
}

bool IsNoCorsSafelistedHeaderNameLowerCase(const std::string& lower_name) {
  if (lower_name != "accept" && lower_name != "accept-language" &&
      lower_name != "content-language" && lower_name != "content-type") {
    return false;
  }
  return true;
}

base::Optional<CorsErrorStatus> CheckAccessInternal(
    const GURL& response_url,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode credentials_mode,
    const url::Origin& origin) {
  if (allow_origin_header == kAsterisk) {
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are
    // to be sent, even with Access-Control-Allow-Credentials set to true.
    // See https://fetch.spec.whatwg.org/#cors-protocol-and-credentials.
    if (credentials_mode != mojom::CredentialsMode::kInclude)
      return base::nullopt;

    // Since the credential is a concept for network schemes, we perform the
    // wildcard check only for HTTP and HTTPS. This is a quick hack to allow
    // data URL (see https://crbug.com/315152).
    // TODO(https://crbug.com/736308): Once the callers exist only in the
    // browser process or network service, this check won't be needed any more
    // because it is always for network requests there.
    if (response_url.SchemeIsHTTPOrHTTPS())
      return CorsErrorStatus(mojom::CorsError::kWildcardOriginNotAllowed);
  } else if (!allow_origin_header) {
    return CorsErrorStatus(mojom::CorsError::kMissingAllowOriginHeader);
  } else if (*allow_origin_header != origin.Serialize()) {
    // We do not use url::Origin::IsSameOriginWith() here for two reasons below.
    //  1. Allow "null" to match here. The latest spec does not have a clear
    //     information about this (https://fetch.spec.whatwg.org/#cors-check),
    //     but the old spec explicitly says that "null" works here
    //     (https://www.w3.org/TR/cors/#resource-sharing-check-0).
    //  2. We do not have a good way to construct url::Origin from the string,
    //     *allow_origin_header, that may be broken. Unfortunately
    //     url::Origin::Create(GURL(*allow_origin_header)) accepts malformed
    //     URL and constructs a valid origin with unexpected fixes, which
    //     results in unexpected behavior.

    // We run some more value checks below to provide better information to
    // developers.

    // Does not allow to have multiple origins in the allow origin header.
    // See https://fetch.spec.whatwg.org/#http-access-control-allow-origin.
    if (allow_origin_header->find_first_of(" ,") != std::string::npos) {
      return CorsErrorStatus(mojom::CorsError::kMultipleAllowOriginValues,
                             *allow_origin_header);
    }

    // Check valid "null" first since GURL assumes it as invalid.
    if (*allow_origin_header == "null") {
      return CorsErrorStatus(mojom::CorsError::kAllowOriginMismatch,
                             *allow_origin_header);
    }

    // As commented above, this validation is not strict as an origin
    // validation, but should be ok for providing error details to developers.
    GURL header_origin(*allow_origin_header);
    if (!header_origin.is_valid()) {
      return CorsErrorStatus(mojom::CorsError::kInvalidAllowOriginValue,
                             *allow_origin_header);
    }

    return CorsErrorStatus(mojom::CorsError::kAllowOriginMismatch,
                           *allow_origin_header);
  }

  if (credentials_mode == mojom::CredentialsMode::kInclude) {
    // https://fetch.spec.whatwg.org/#http-access-control-allow-credentials.
    // This check should be case sensitive.
    // See also https://fetch.spec.whatwg.org/#http-new-header-syntax.
    if (allow_credentials_header != kLowerCaseTrue) {
      return CorsErrorStatus(mojom::CorsError::kInvalidAllowCredentials,
                             allow_credentials_header.value_or(std::string()));
    }
  }
  return base::nullopt;
}

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

void ReportAccessCheckResultMetric(AccessCheckResult result) {
  UMA_HISTOGRAM_ENUMERATION("Net.Cors.AccessCheckResult", result);
}

}  // namespace

namespace cors {

namespace header_names {

const char kAccessControlAllowCredentials[] =
    "Access-Control-Allow-Credentials";
const char kAccessControlAllowExternal[] = "Access-Control-Allow-External";
const char kAccessControlAllowHeaders[] = "Access-Control-Allow-Headers";
const char kAccessControlAllowMethods[] = "Access-Control-Allow-Methods";
const char kAccessControlAllowOrigin[] = "Access-Control-Allow-Origin";
const char kAccessControlMaxAge[] = "Access-Control-Max-Age";
const char kAccessControlRequestExternal[] = "Access-Control-Request-External";
const char kAccessControlRequestHeaders[] = "Access-Control-Request-Headers";
const char kAccessControlRequestMethod[] = "Access-Control-Request-Method";

}  // namespace header_names

// See https://fetch.spec.whatwg.org/#cors-check.
base::Optional<CorsErrorStatus> CheckAccess(
    const GURL& response_url,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode credentials_mode,
    const url::Origin& origin) {
  const auto error_status =
      CheckAccessInternal(response_url, allow_origin_header,
                          allow_credentials_header, credentials_mode, origin);
  ReportAccessCheckResultMetric(error_status ? AccessCheckResult::kNotPermitted
                                             : AccessCheckResult::kPermitted);
  if (error_status) {
    UMA_HISTOGRAM_ENUMERATION("Net.Cors.AccessCheckError",
                              error_status->cors_error);
  }
  return error_status;
}

bool ShouldCheckCors(const GURL& request_url,
                     const base::Optional<url::Origin>& request_initiator,
                     mojom::RequestMode request_mode) {
  if (request_mode == network::mojom::RequestMode::kNavigate ||
      request_mode == network::mojom::RequestMode::kNoCors) {
    return false;
  }

  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS should not work.
  DCHECK(request_initiator);

  // |request_url| should not contain the url::kDataScheme here, but have a
  // DCHECK for a while, just in case.
  DCHECK(!request_url.SchemeIs(url::kDataScheme));

  if (request_initiator->IsSameOriginWith(url::Origin::Create(request_url)))
    return false;
  return true;
}

base::Optional<CorsErrorStatus> CheckPreflightAccess(
    const GURL& response_url,
    const int response_status_code,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode actual_credentials_mode,
    const url::Origin& origin) {
  // Step 7 of https://fetch.spec.whatwg.org/#cors-preflight-fetch
  auto error_status = CheckAccessInternal(response_url, allow_origin_header,
                                          allow_credentials_header,
                                          actual_credentials_mode, origin);
  const bool has_ok_status = IsOkStatus(response_status_code);

  ReportAccessCheckResultMetric(
      (error_status || !has_ok_status)
          ? AccessCheckResult::kNotPermittedInPreflight
          : AccessCheckResult::kPermittedInPreflight);

  // Prefer using a preflight specific error code.
  if (error_status) {
    switch (error_status->cors_error) {
      case mojom::CorsError::kWildcardOriginNotAllowed:
        error_status->cors_error =
            mojom::CorsError::kPreflightWildcardOriginNotAllowed;
        break;
      case mojom::CorsError::kMissingAllowOriginHeader:
        error_status->cors_error =
            mojom::CorsError::kPreflightMissingAllowOriginHeader;
        break;
      case mojom::CorsError::kMultipleAllowOriginValues:
        error_status->cors_error =
            mojom::CorsError::kPreflightMultipleAllowOriginValues;
        break;
      case mojom::CorsError::kInvalidAllowOriginValue:
        error_status->cors_error =
            mojom::CorsError::kPreflightInvalidAllowOriginValue;
        break;
      case mojom::CorsError::kAllowOriginMismatch:
        error_status->cors_error =
            mojom::CorsError::kPreflightAllowOriginMismatch;
        break;
      case mojom::CorsError::kInvalidAllowCredentials:
        error_status->cors_error =
            mojom::CorsError::kPreflightInvalidAllowCredentials;
        break;
      default:
        NOTREACHED();
        break;
    }
  } else if (!has_ok_status) {
    error_status = base::make_optional<CorsErrorStatus>(
        mojom::CorsError::kPreflightInvalidStatus);
  } else {
    return base::nullopt;
  }

  UMA_HISTOGRAM_ENUMERATION("Net.Cors.PreflightCheckError",
                            error_status->cors_error);
  return error_status;
}

base::Optional<CorsErrorStatus> CheckRedirectLocation(
    const GURL& url,
    mojom::RequestMode request_mode,
    const base::Optional<url::Origin>& origin,
    bool cors_flag,
    bool tainted) {
  // If |actualResponse|’s location URL’s scheme is not an HTTP(S) scheme,
  // then return a network error.
  // This should be addressed in //net.

  // Note: The redirect count check is done elsewhere.

  const bool url_has_credentials = url.has_username() || url.has_password();
  // If |request|’s mode is "cors", |actualResponse|’s location URL includes
  // credentials, and either |request|’s tainted origin flag is set or
  // |request|’s origin is not same origin with |actualResponse|’s location
  // URL’s origin, then return a network error.
  DCHECK(!IsCorsEnabledRequestMode(request_mode) || origin);
  if (IsCorsEnabledRequestMode(request_mode) && url_has_credentials &&
      (tainted || !origin->IsSameOriginWith(url::Origin::Create(url)))) {
    return CorsErrorStatus(mojom::CorsError::kRedirectContainsCredentials);
  }

  // If CORS flag is set and |actualResponse|’s location URL includes
  // credentials, then return a network error.
  if (cors_flag && url_has_credentials)
    return CorsErrorStatus(mojom::CorsError::kRedirectContainsCredentials);

  return base::nullopt;
}

// https://wicg.github.io/cors-rfc1918/#http-headerdef-access-control-allow-external
base::Optional<CorsErrorStatus> CheckExternalPreflight(
    const base::Optional<std::string>& allow_external) {
  if (!allow_external)
    return CorsErrorStatus(mojom::CorsError::kPreflightMissingAllowExternal);
  if (*allow_external == kLowerCaseTrue)
    return base::nullopt;
  return CorsErrorStatus(mojom::CorsError::kPreflightInvalidAllowExternal,
                         *allow_external);
}

bool IsCorsEnabledRequestMode(mojom::RequestMode mode) {
  return mode == mojom::RequestMode::kCors ||
         mode == mojom::RequestMode::kCorsWithForcedPreflight;
}

bool IsCorsSafelistedMethod(const std::string& method) {
  // https://fetch.spec.whatwg.org/#cors-safelisted-method
  // "A CORS-safelisted method is a method that is `GET`, `HEAD`, or `POST`."
  std::string method_upper = base::ToUpperASCII(method);
  return method_upper == net::HttpRequestHeaders::kGetMethod ||
         method_upper == net::HttpRequestHeaders::kHeadMethod ||
         method_upper == net::HttpRequestHeaders::kPostMethod;
}

bool IsCorsSafelistedContentType(const std::string& media_type) {
  return IsCorsSafelistedLowerCaseContentType(base::ToLowerASCII(media_type));
}

bool IsCorsSafelistedHeader(const std::string& name, const std::string& value) {
  const std::string lower_name = base::ToLowerASCII(name);

  // If |value|’s length is greater than 128, then return false.
  if (value.size() > 128)
    return false;

  // https://fetch.spec.whatwg.org/#cors-safelisted-request-header
  // "A CORS-safelisted header is a header whose name is either one of `Accept`,
  // `Accept-Language`, and `Content-Language`, or whose name is
  // `Content-Type` and value, once parsed, is one of
  //     `application/x-www-form-urlencoded`, `multipart/form-data`, and
  //     `text/plain`
  // or whose name is a byte-case-insensitive match for one of
  //      `DPR`, `Save-Data`, `device-memory`, `Viewport-Width`, and `Width`,
  // and whose value, once extracted, is not failure."
  //
  // Treat inspector headers as a CORS-safelisted headers, since they are added
  // by blink when the inspector is open.
  //
  // Treat 'Intervention' as a CORS-safelisted header, since it is added by
  // Chrome when an intervention is (or may be) applied.
  static const char* const safe_names[] = {
      "accept",
      "accept-language",
      "content-language",
      "intervention",
      "content-type",
      "save-data",
      // The Device Memory header field is a number that indicates the client’s
      // device memory i.e. approximate amount of ram in GiB. The header value
      // must satisfy ABNF  1*DIGIT [ "." 1*DIGIT ]
      // See
      // https://w3c.github.io/device-memory/#sec-device-memory-client-hint-header
      // for more details.
      "device-memory",
      "dpr",
      "width",
      "viewport-width",

      // The `Sec-CH-Lang` header field is a proposed replacement for
      // `Accept-Language`, using the Client Hints infrastructure.
      //
      // https://tools.ietf.org/html/draft-west-lang-client-hint
      "sec-ch-lang",

      // The `Sec-CH-UA-*` header fields are proposed replacements for
      // `User-Agent`, using the Client Hints infrastructure.
      //
      // https://tools.ietf.org/html/draft-west-ua-client-hints
      "sec-ch-ua",
      "sec-ch-ua-platform",
      "sec-ch-ua-arch",
      "sec-ch-ua-model",
      "sec-ch-ua-mobile",
      "sec-ch-ua-full-version",
      "sec-ch-ua-platform-version",
  };
  if (std::find(std::begin(safe_names), std::end(safe_names), lower_name) ==
      std::end(safe_names))
    return false;

  // Client hints are device specific, and not origin specific. As such all
  // client hint headers are considered as safe.
  // See
  // third_party/blink/public/mojom/web_client_hints/web_client_hints_types.mojom.
  // Client hint headers can be added by Chrome automatically or via JavaScript.
  if (lower_name == "device-memory" || lower_name == "dpr")
    return IsSimilarToDoubleABNF(value);
  if (lower_name == "width" || lower_name == "viewport-width")
    return IsSimilarToIntABNF(value);
  const std::string lower_value = base::ToLowerASCII(value);
  if (lower_name == "save-data")
    return lower_value == "on";

  if (lower_name == "accept") {
    return !std::any_of(value.begin(), value.end(),
                        IsCorsUnsafeRequestHeaderByte);
  }

  if (lower_name == "accept-language" || lower_name == "content-language") {
    return std::all_of(value.begin(), value.end(), [](char c) {
      return (0x30 <= c && c <= 0x39) || (0x41 <= c && c <= 0x5a) ||
             (0x61 <= c && c <= 0x7a) || c == 0x20 || c == 0x2a || c == 0x2c ||
             c == 0x2d || c == 0x2e || c == 0x3b || c == 0x3d;
    });
  }

  if (lower_name == "content-type")
    return IsCorsSafelistedLowerCaseContentType(lower_value);

  return true;
}

bool IsNoCorsSafelistedHeaderName(const std::string& name) {
  return IsNoCorsSafelistedHeaderNameLowerCase(base::ToLowerASCII(name));
}

bool IsPrivilegedNoCorsHeaderName(const std::string& name) {
  const std::string lower_name = base::ToLowerASCII(name);
  const std::vector<std::string> privileged_no_cors_header_names =
      PrivilegedNoCorsHeaderNames();
  for (const auto& header : privileged_no_cors_header_names) {
    if (lower_name == header)
      return true;
  }
  return false;
}

bool IsNoCorsSafelistedHeader(const std::string& name,
                              const std::string& value) {
  const std::string lower_name = base::ToLowerASCII(name);

  if (!IsNoCorsSafelistedHeaderNameLowerCase(lower_name))
    return false;
  return IsCorsSafelistedHeader(lower_name, value);
}

std::vector<std::string> CorsUnsafeRequestHeaderNames(
    const net::HttpRequestHeaders::HeaderVector& headers) {
  std::vector<std::string> potentially_unsafe_names;
  std::vector<std::string> header_names;

  constexpr size_t kSafeListValueSizeMax = 1024;
  size_t safe_list_value_size = 0;

  for (const auto& header : headers) {
    if (!IsCorsSafelistedHeader(header.key, header.value)) {
      header_names.push_back(base::ToLowerASCII(header.key));
    } else {
      potentially_unsafe_names.push_back(base::ToLowerASCII(header.key));
      safe_list_value_size += header.value.size();
    }
  }
  if (safe_list_value_size > kSafeListValueSizeMax) {
    header_names.insert(header_names.end(), potentially_unsafe_names.begin(),
                        potentially_unsafe_names.end());
  }
  return header_names;
}

std::vector<std::string> CorsUnsafeNotForbiddenRequestHeaderNames(
    const net::HttpRequestHeaders::HeaderVector& headers,
    bool is_revalidating) {
  std::vector<std::string> header_names;
  std::vector<std::string> potentially_unsafe_names;

  constexpr size_t kSafeListValueSizeMax = 1024;
  size_t safe_list_value_size = 0;

  for (const auto& header : headers) {
    if (!net::HttpUtil::IsSafeHeader(header.key))
      continue;

    const std::string name = base::ToLowerASCII(header.key);

    if (is_revalidating) {
      if (name == "if-modified-since" || name == "if-none-match" ||
          name == "cache-control") {
        continue;
      }
    }
    if (!IsCorsSafelistedHeader(name, header.value)) {
      header_names.push_back(name);
    } else {
      potentially_unsafe_names.push_back(name);
      safe_list_value_size += header.value.size();
    }
  }
  if (safe_list_value_size > kSafeListValueSizeMax) {
    header_names.insert(header_names.end(), potentially_unsafe_names.begin(),
                        potentially_unsafe_names.end());
  }
  return header_names;
}

std::vector<std::string> PrivilegedNoCorsHeaderNames() {
  return {"range"};
}

bool IsForbiddenMethod(const std::string& method) {
  const std::string upper_method = base::ToUpperASCII(method);
  return upper_method == net::HttpRequestHeaders::kConnectMethod ||
         upper_method == net::HttpRequestHeaders::kTraceMethod ||
         upper_method == net::HttpRequestHeaders::kTrackMethod;
}

bool IsOkStatus(int status) {
  return status >= 200 && status < 300;
}

bool IsCorsSameOriginResponseType(mojom::FetchResponseType type) {
  switch (type) {
    case mojom::FetchResponseType::kBasic:
    case mojom::FetchResponseType::kCors:
    case mojom::FetchResponseType::kDefault:
      return true;
    case mojom::FetchResponseType::kError:
    case mojom::FetchResponseType::kOpaque:
    case mojom::FetchResponseType::kOpaqueRedirect:
      return false;
  }
}

bool IsCorsCrossOriginResponseType(mojom::FetchResponseType type) {
  switch (type) {
    case mojom::FetchResponseType::kBasic:
    case mojom::FetchResponseType::kCors:
    case mojom::FetchResponseType::kDefault:
    case mojom::FetchResponseType::kError:
      return false;
    case mojom::FetchResponseType::kOpaque:
    case mojom::FetchResponseType::kOpaqueRedirect:
      return true;
  }
}

bool CalculateCredentialsFlag(mojom::CredentialsMode credentials_mode,
                              mojom::FetchResponseType response_tainting) {
  // Let |credentials flag| be set if one of
  //  - |request|’s credentials mode is "include"
  //  - |request|’s credentials mode is "same-origin" and |request|’s
  //    response tainting is "basic"
  // is true, and unset otherwise.
  switch (credentials_mode) {
    case network::mojom::CredentialsMode::kOmit:
    case network::mojom::CredentialsMode::kOmitBug_775438_Workaround:
      return false;
    case network::mojom::CredentialsMode::kSameOrigin:
      return response_tainting == network::mojom::FetchResponseType::kBasic;
    case network::mojom::CredentialsMode::kInclude:
      return true;
  }
}

mojom::FetchResponseType CalculateResponseType(
    mojom::RequestMode mode,
    bool is_request_considered_same_origin) {
  if (is_request_considered_same_origin ||
      mode == network::mojom::RequestMode::kNavigate ||
      mode == network::mojom::RequestMode::kSameOrigin) {
    return network::mojom::FetchResponseType::kBasic;
  } else if (mode == network::mojom::RequestMode::kNoCors) {
    return network::mojom::FetchResponseType::kOpaque;
  } else {
    DCHECK(network::cors::IsCorsEnabledRequestMode(mode)) << mode;
    return network::mojom::FetchResponseType::kCors;
  }
}

}  // namespace cors

}  // namespace network
