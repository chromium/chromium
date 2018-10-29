// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/mime_util.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace {

const char kAsterisk[] = "*";
const char kLowerCaseTrue[] = "true";

// TODO(toyoshim): Consider to move following const variables to
// //net/http/http_request_headers.
const char kHeadMethod[] = "HEAD";
const char kPostMethod[] = "POST";

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

// |lower_case_media_type| should be lower case.
bool IsCORSSafelistedLowerCaseContentType(
    const std::string& lower_case_media_type) {
  DCHECK_EQ(lower_case_media_type, base::ToLowerASCII(lower_case_media_type));
  std::string mime_type = ExtractMIMETypeFromMediaType(lower_case_media_type);
  return mime_type == "application/x-www-form-urlencoded" ||
         mime_type == "multipart/form-data" || mime_type == "text/plain";
}

}  // namespace

namespace network {

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
base::Optional<CORSErrorStatus> CheckAccess(
    const GURL& response_url,
    const int response_status_code,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_credentials_header,
    mojom::FetchCredentialsMode credentials_mode,
    const url::Origin& origin) {
  // TODO(toyoshim): This response status code check should not be needed. We
  // have another status code check after a CheckAccess() call if it is needed.
  if (!response_status_code)
    return CORSErrorStatus(mojom::CORSError::kInvalidResponse);

  if (allow_origin_header == kAsterisk) {
    // A wildcard Access-Control-Allow-Origin can not be used if credentials are
    // to be sent, even with Access-Control-Allow-Credentials set to true.
    // See https://fetch.spec.whatwg.org/#cors-protocol-and-credentials.
    if (credentials_mode != mojom::FetchCredentialsMode::kInclude)
      return base::nullopt;

    // Since the credential is a concept for network schemes, we perform the
    // wildcard check only for HTTP and HTTPS. This is a quick hack to allow
    // data URL (see https://crbug.com/315152).
    // TODO(https://crbug.com/736308): Once the callers exist only in the
    // browser process or network service, this check won't be needed any more
    // because it is always for network requests there.
    if (response_url.SchemeIsHTTPOrHTTPS())
      return CORSErrorStatus(mojom::CORSError::kWildcardOriginNotAllowed);
  } else if (!allow_origin_header) {
    return CORSErrorStatus(mojom::CORSError::kMissingAllowOriginHeader);
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
      return CORSErrorStatus(mojom::CORSError::kMultipleAllowOriginValues,
                             *allow_origin_header);
    }

    // Check valid "null" first since GURL assumes it as invalid.
    if (*allow_origin_header == "null") {
      return CORSErrorStatus(mojom::CORSError::kAllowOriginMismatch,
                             *allow_origin_header);
    }

    // As commented above, this validation is not strict as an origin
    // validation, but should be ok for providing error details to developers.
    GURL header_origin(*allow_origin_header);
    if (!header_origin.is_valid()) {
      return CORSErrorStatus(mojom::CORSError::kInvalidAllowOriginValue,
                             *allow_origin_header);
    }

    return CORSErrorStatus(mojom::CORSError::kAllowOriginMismatch,
                           *allow_origin_header);
  }

  if (credentials_mode == mojom::FetchCredentialsMode::kInclude) {
    // https://fetch.spec.whatwg.org/#http-access-control-allow-credentials.
    // This check should be case sensitive.
    // See also https://fetch.spec.whatwg.org/#http-new-header-syntax.
    if (allow_credentials_header != kLowerCaseTrue) {
      return CORSErrorStatus(mojom::CORSError::kInvalidAllowCredentials,
                             allow_credentials_header.value_or(std::string()));
    }
  }
  return base::nullopt;
}

base::Optional<CORSErrorStatus> CheckPreflightAccess(
    const GURL& response_url,
    const int response_status_code,
    const base::Optional<std::string>& allow_origin_header,
    const base::Optional<std::string>& allow_credentials_header,
    mojom::FetchCredentialsMode actual_credentials_mode,
    const url::Origin& origin) {
  const auto error_status =
      CheckAccess(response_url, response_status_code, allow_origin_header,
                  allow_credentials_header, actual_credentials_mode, origin);
  if (!error_status)
    return base::nullopt;

  // TODO(toyoshim): Remove following two lines when the status code check is
  // removed from CheckAccess().
  if (error_status->cors_error == mojom::CORSError::kInvalidResponse)
    return error_status;

  mojom::CORSError error = error_status->cors_error;
  switch (error_status->cors_error) {
    case mojom::CORSError::kWildcardOriginNotAllowed:
      error = mojom::CORSError::kPreflightWildcardOriginNotAllowed;
      break;
    case mojom::CORSError::kMissingAllowOriginHeader:
      error = mojom::CORSError::kPreflightMissingAllowOriginHeader;
      break;
    case mojom::CORSError::kMultipleAllowOriginValues:
      error = mojom::CORSError::kPreflightMultipleAllowOriginValues;
      break;
    case mojom::CORSError::kInvalidAllowOriginValue:
      error = mojom::CORSError::kPreflightInvalidAllowOriginValue;
      break;
    case mojom::CORSError::kAllowOriginMismatch:
      error = mojom::CORSError::kPreflightAllowOriginMismatch;
      break;
    case mojom::CORSError::kInvalidAllowCredentials:
      error = mojom::CORSError::kPreflightInvalidAllowCredentials;
      break;
    default:
      NOTREACHED();
      break;
  }
  return CORSErrorStatus(error, error_status->failed_parameter);
}

base::Optional<CORSErrorStatus> CheckRedirectLocation(
    const GURL& url,
    mojom::FetchRequestMode request_mode,
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
  DCHECK(!IsCORSEnabledRequestMode(request_mode) || origin);
  if (IsCORSEnabledRequestMode(request_mode) && url_has_credentials &&
      (tainted || !origin->IsSameOriginWith(url::Origin::Create(url)))) {
    return CORSErrorStatus(mojom::CORSError::kRedirectContainsCredentials);
  }

  // If CORS flag is set and |actualResponse|’s location URL includes
  // credentials, then return a network error.
  if (cors_flag && url_has_credentials)
    return CORSErrorStatus(mojom::CORSError::kRedirectContainsCredentials);

  return base::nullopt;
}

base::Optional<mojom::CORSError> CheckPreflight(const int status_code) {
  // CORS preflight with 3XX is considered network error in
  // Fetch API Spec: https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // CORS Spec: http://www.w3.org/TR/cors/#cross-origin-request-with-preflight-0
  // https://crbug.com/452394
  if (IsOkStatus(status_code))
    return base::nullopt;
  return mojom::CORSError::kPreflightInvalidStatus;
}

// https://wicg.github.io/cors-rfc1918/#http-headerdef-access-control-allow-external
base::Optional<CORSErrorStatus> CheckExternalPreflight(
    const base::Optional<std::string>& allow_external) {
  if (!allow_external)
    return CORSErrorStatus(mojom::CORSError::kPreflightMissingAllowExternal);
  if (*allow_external == kLowerCaseTrue)
    return base::nullopt;
  return CORSErrorStatus(mojom::CORSError::kPreflightInvalidAllowExternal,
                         *allow_external);
}

bool IsCORSEnabledRequestMode(mojom::FetchRequestMode mode) {
  return mode == mojom::FetchRequestMode::kCORS ||
         mode == mojom::FetchRequestMode::kCORSWithForcedPreflight;
}

mojom::FetchResponseType CalculateResponseTainting(
    const GURL& url,
    mojom::FetchRequestMode request_mode,
    const base::Optional<url::Origin>& origin,
    bool cors_flag) {
  if (url.SchemeIs(url::kDataScheme))
    return mojom::FetchResponseType::kBasic;

  if (cors_flag) {
    DCHECK(IsCORSEnabledRequestMode(request_mode));
    return mojom::FetchResponseType::kCORS;
  }

  if (!origin) {
    // This is actually not defined in the fetch spec, but in this case CORS
    // is disabled so no one should care this value.
    return mojom::FetchResponseType::kBasic;
  }

  if (request_mode == mojom::FetchRequestMode::kNoCORS &&
      !origin->IsSameOriginWith(url::Origin::Create(url))) {
    return mojom::FetchResponseType::kOpaque;
  }
  return mojom::FetchResponseType::kBasic;
}

bool IsCORSSafelistedMethod(const std::string& method) {
  // https://fetch.spec.whatwg.org/#cors-safelisted-method
  // "A CORS-safelisted method is a method that is `GET`, `HEAD`, or `POST`."
  std::string method_upper = base::ToUpperASCII(method);
  return method_upper == net::HttpRequestHeaders::kGetMethod ||
         method_upper == kHeadMethod || method_upper == kPostMethod;
}

bool IsCORSSafelistedContentType(const std::string& media_type) {
  return IsCORSSafelistedLowerCaseContentType(base::ToLowerASCII(media_type));
}

bool IsCORSSafelistedHeader(const std::string& name, const std::string& value) {
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
      "accept", "accept-language", "content-language", "intervention",
      "content-type", "save-data",
      // The Device Memory header field is a number that indicates the client’s
      // device memory i.e. approximate amount of ram in GiB. The header value
      // must satisfy ABNF  1*DIGIT [ "." 1*DIGIT ]
      // See
      // https://w3c.github.io/device-memory/#sec-device-memory-client-hint-header
      // for more details.
      "device-memory", "dpr", "width", "viewport-width"};
  const std::string lower_name = base::ToLowerASCII(name);
  if (std::find(std::begin(safe_names), std::end(safe_names), lower_name) ==
      std::end(safe_names))
    return false;

  // Client hints are device specific, and not origin specific. As such all
  // client hint headers are considered as safe.
  // See third_party/WebKit/public/platform/web_client_hints_types.mojom.
  // Client hint headers can be added by Chrome automatically or via JavaScript.
  if (lower_name == "device-memory" || lower_name == "dpr")
    return IsSimilarToDoubleABNF(value);
  if (lower_name == "width" || lower_name == "viewport-width")
    return IsSimilarToIntABNF(value);
  const std::string lower_value = base::ToLowerASCII(value);
  if (lower_name == "save-data")
    return lower_value == "on";

  if (lower_name == "accept") {
    return (value.end() == std::find_if(value.begin(), value.end(), [](char c) {
              return (c < 0x20 && c != 0x09) || c == 0x22 || c == 0x28 ||
                     c == 0x29 || c == 0x3a || c == 0x3c || c == 0x3e ||
                     c == 0x3f || c == 0x40 || c == 0x5b || c == 0x5c ||
                     c == 0x5d || c == 0x7b || c == 0x7d || c >= 0x7f;
            }));
  }

  if (lower_name == "accept-language" || lower_name == "content-language") {
    return (value.end() == std::find_if(value.begin(), value.end(), [](char c) {
              return !isalnum(c) && c != 0x20 && c != 0x2a && c != 0x2c &&
                     c != 0x2d && c != 0x2e && c != 0x3b && c != 0x3d;
            }));
  }

  if (lower_name == "content-type")
    return IsCORSSafelistedLowerCaseContentType(lower_value);

  return true;
}

bool IsNoCORSSafelistedHeader(const std::string& name,
                              const std::string& value) {
  const std::string lower_name = base::ToLowerASCII(name);

  if (lower_name != "accept" && lower_name != "accept-language" &&
      lower_name != "content-language" && lower_name != "content-type") {
    return false;
  }

  return IsCORSSafelistedHeader(lower_name, value);
}

std::vector<std::string> CORSUnsafeRequestHeaderNames(
    const net::HttpRequestHeaders::HeaderVector& headers) {
  std::vector<std::string> potentially_unsafe_names;
  std::vector<std::string> header_names;

  constexpr size_t kSafeListValueSizeMax = 1024;
  size_t safe_list_value_size = 0;

  for (const auto& header : headers) {
    if (!IsCORSSafelistedHeader(header.key, header.value)) {
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

std::vector<std::string> CORSUnsafeNotForbiddenRequestHeaderNames(
    const net::HttpRequestHeaders::HeaderVector& headers,
    bool is_revalidating) {
  std::vector<std::string> header_names;
  std::vector<std::string> potentially_unsafe_names;

  constexpr size_t kSafeListValueSizeMax = 1024;
  size_t safe_list_value_size = 0;

  for (const auto& header : headers) {
    if (IsForbiddenHeader(header.key))
      continue;

    const std::string name = base::ToLowerASCII(header.key);

    if (is_revalidating) {
      if (name == "if-modified-since" || name == "if-none-match" ||
          name == "cache-control") {
        continue;
      }
    }
    if (!IsCORSSafelistedHeader(name, header.value)) {
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

bool IsForbiddenMethod(const std::string& method) {
  const std::string lower_method = base::ToLowerASCII(method);
  return lower_method == "trace" || lower_method == "track" ||
         lower_method == "connect";
}

bool IsForbiddenHeader(const std::string& name) {
  // http://fetch.spec.whatwg.org/#forbidden-header-name
  // "A forbidden header name is a header name that is one of:
  //   `Accept-Charset`, `Accept-Encoding`, `Access-Control-Request-Headers`,
  //   `Access-Control-Request-Method`, `Connection`, `Content-Length`,
  //   `Cookie`, `Cookie2`, `Date`, `DNT`, `Expect`, `Host`, `Keep-Alive`,
  //   `Origin`, `Referer`, `TE`, `Trailer`, `Transfer-Encoding`, `Upgrade`,
  //   `User-Agent`, `Via`
  // or starts with `Proxy-` or `Sec-` (including when it is just `Proxy-` or
  // `Sec-`)."
  static const base::NoDestructor<base::flat_set<base::StringPiece>>
      kForbiddenNames(
          base::flat_set<base::StringPiece>{"accept-charset",
                                            "accept-encoding",
                                            "access-control-request-headers",
                                            "access-control-request-method",
                                            "connection",
                                            "content-length",
                                            "cookie",
                                            "cookie2",
                                            "date",
                                            "dnt",
                                            "expect",
                                            "host",
                                            "keep-alive",
                                            "origin",
                                            "referer",
                                            "te",
                                            "trailer",
                                            "transfer-encoding",
                                            "upgrade",
                                            "user-agent",
                                            "via"});
  const std::string lower_name = base::ToLowerASCII(name);
  if (StartsWith(lower_name, "proxy-", base::CompareCase::SENSITIVE) ||
      StartsWith(lower_name, "sec-", base::CompareCase::SENSITIVE)) {
    return true;
  }
  return kForbiddenNames->contains(lower_name);
}

bool IsOkStatus(int status) {
  return status >= 200 && status < 300;
}

bool IsCORSSameOriginResponseType(mojom::FetchResponseType type) {
  switch (type) {
    case mojom::FetchResponseType::kBasic:
    case mojom::FetchResponseType::kCORS:
    case mojom::FetchResponseType::kDefault:
      return true;
    case mojom::FetchResponseType::kError:
    case mojom::FetchResponseType::kOpaque:
    case mojom::FetchResponseType::kOpaqueRedirect:
      return false;
  }
}

bool IsCORSCrossOriginResponseType(mojom::FetchResponseType type) {
  switch (type) {
    case mojom::FetchResponseType::kBasic:
    case mojom::FetchResponseType::kCORS:
    case mojom::FetchResponseType::kDefault:
    case mojom::FetchResponseType::kError:
      return false;
    case mojom::FetchResponseType::kOpaque:
    case mojom::FetchResponseType::kOpaqueRedirect:
      return true;
  }
}

bool CalculateCredentialsFlag(mojom::FetchCredentialsMode credentials_mode,
                              mojom::FetchResponseType response_tainting) {
  // Let |credentials flag| be set if one of
  //  - |request|’s credentials mode is "include"
  //  - |request|’s credentials mode is "same-origin" and |request|’s
  //    response tainting is "basic"
  // is true, and unset otherwise.
  switch (credentials_mode) {
    case network::mojom::FetchCredentialsMode::kOmit:
      return false;
    case network::mojom::FetchCredentialsMode::kSameOrigin:
      return response_tainting == network::mojom::FetchResponseType::kBasic;
    case network::mojom::FetchCredentialsMode::kInclude:
      return true;
  }
}

}  // namespace cors

}  // namespace network
