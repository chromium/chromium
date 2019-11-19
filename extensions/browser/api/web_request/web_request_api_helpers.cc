// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_api_helpers.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/extension_messages.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log_event_type.h"
#include "services/network/public/cpp/features.h"
#include "url/url_constants.h"

// TODO(battre): move all static functions into an anonymous namespace at the
// top of this file.

using base::Time;
using net::cookie_util::ParsedRequestCookie;
using net::cookie_util::ParsedRequestCookies;

namespace keys = extension_web_request_api_constants;
namespace web_request = extensions::api::web_request;

namespace extension_web_request_api_helpers {

namespace {

using ParsedResponseCookies = std::vector<std::unique_ptr<net::ParsedCookie>>;

void ClearCacheOnNavigationOnUI() {
  web_cache::WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}

bool ParseCookieLifetime(const net::ParsedCookie& cookie,
                         int64_t* seconds_till_expiry) {
  // 'Max-Age' is processed first because according to:
  // http://tools.ietf.org/html/rfc6265#section-5.3 'Max-Age' attribute
  // overrides 'Expires' attribute.
  if (cookie.HasMaxAge() &&
      base::StringToInt64(cookie.MaxAge(), seconds_till_expiry)) {
    return true;
  }

  Time parsed_expiry_time;
  if (cookie.HasExpires()) {
    parsed_expiry_time =
        net::cookie_util::ParseCookieExpirationTime(cookie.Expires());
  }

  if (!parsed_expiry_time.is_null()) {
    *seconds_till_expiry =
        ceil((parsed_expiry_time - Time::Now()).InSecondsF());
    return *seconds_till_expiry >= 0;
  }
  return false;
}

std::set<std::string> GetExtraHeaderRequestHeaders(
    bool is_out_of_blink_cors_enabled) {
  std::set<std::string> headers(
      {"accept-encoding", "accept-language", "cookie", "referer"});

  if (is_out_of_blink_cors_enabled)
    headers.insert("origin");

  return headers;
}

void RecordRequestHeaderRemoved(RequestHeaderType type) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.RequestHeaderRemoved", type);
}

void RecordRequestHeaderAdded(RequestHeaderType type) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.RequestHeaderAdded", type);
}

void RecordRequestHeaderChanged(RequestHeaderType type) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.RequestHeaderChanged", type);
}

bool IsStringLowerCaseASCII(base::StringPiece s) {
  return std::none_of(s.begin(), s.end(), base::IsAsciiUpper<char>);
}

using RequestHeaderEntry = std::pair<const char*, RequestHeaderType>;
constexpr RequestHeaderEntry kRequestHeaderEntries[] = {
    {"accept", RequestHeaderType::kAccept},
    {"accept-charset", RequestHeaderType::kAcceptCharset},
    {"accept-encoding", RequestHeaderType::kAcceptEncoding},
    {"accept-language", RequestHeaderType::kAcceptLanguage},
    {"access-control-request-headers",
     RequestHeaderType::kAccessControlRequestHeaders},
    {"access-control-request-method",
     RequestHeaderType::kAccessControlRequestMethod},
    {"authorization", RequestHeaderType::kAuthorization},
    {"cache-control", RequestHeaderType::kCacheControl},
    {"connection", RequestHeaderType::kConnection},
    {"content-encoding", RequestHeaderType::kContentEncoding},
    {"content-language", RequestHeaderType::kContentLanguage},
    {"content-length", RequestHeaderType::kContentLength},
    {"content-location", RequestHeaderType::kContentLocation},
    {"content-type", RequestHeaderType::kContentType},
    {"cookie", RequestHeaderType::kCookie},
    {"date", RequestHeaderType::kDate},
    {"dnt", RequestHeaderType::kDnt},
    {"early-data", RequestHeaderType::kEarlyData},
    {"expect", RequestHeaderType::kExpect},
    {"forwarded", RequestHeaderType::kForwarded},
    {"from", RequestHeaderType::kFrom},
    {"host", RequestHeaderType::kHost},
    {"if-match", RequestHeaderType::kIfMatch},
    {"if-modified-since", RequestHeaderType::kIfModifiedSince},
    {"if-none-match", RequestHeaderType::kIfNoneMatch},
    {"if-range", RequestHeaderType::kIfRange},
    {"if-unmodified-since", RequestHeaderType::kIfUnmodifiedSince},
    {"keep-alive", RequestHeaderType::kKeepAlive},
    {"origin", RequestHeaderType::kOrigin},
    {"pragma", RequestHeaderType::kPragma},
    {"proxy-authorization", RequestHeaderType::kProxyAuthorization},
    {"proxy-connection", RequestHeaderType::kProxyConnection},
    {"range", RequestHeaderType::kRange},
    {"referer", RequestHeaderType::kReferer},
    {"sec-origin-policy", RequestHeaderType::kSecOriginPolicy},
    {"te", RequestHeaderType::kTe},
    {"transfer-encoding", RequestHeaderType::kTransferEncoding},
    {"upgrade", RequestHeaderType::kUpgrade},
    {"upgrade-insecure-requests", RequestHeaderType::kUpgradeInsecureRequests},
    {"user-agent", RequestHeaderType::kUserAgent},
    {"via", RequestHeaderType::kVia},
    {"warning", RequestHeaderType::kWarning},
    {"x-forwarded-for", RequestHeaderType::kXForwardedFor},
    {"x-forwarded-host", RequestHeaderType::kXForwardedHost},
    {"x-forwarded-proto", RequestHeaderType::kXForwardedProto}};

constexpr bool IsValidHeaderName(const char* str) {
  while (*str) {
    if ((*str >= 'a' && *str <= 'z') || *str == '-') {
      str++;
      continue;
    }
    return false;
  }
  return true;
}

template <typename T>
constexpr bool ValidateHeaderEntries(const T& entries) {
  for (size_t i = 0; i < base::size(entries); ++i) {
    if (!IsValidHeaderName(entries[i].first))
      return false;
  }
  return true;
}

// All entries other than kOther and kNone are mapped.
static_assert(static_cast<size_t>(RequestHeaderType::kMaxValue) - 1 ==
                  base::size(kRequestHeaderEntries),
              "Invalid number of request header entries");

static_assert(ValidateHeaderEntries(kRequestHeaderEntries),
              "Invalid request header entries");

// Uses |record_func| to record |header|. If |header| is not recorded, false is
// returned.
void RecordRequestHeader(const std::string& header,
                         void (*record_func)(RequestHeaderType)) {
  using HeaderMapType = base::flat_map<base::StringPiece, RequestHeaderType>;
  static const base::NoDestructor<HeaderMapType> kHeaderMap([] {
    std::vector<std::pair<base::StringPiece, RequestHeaderType>> entries;
    entries.reserve(base::size(kRequestHeaderEntries));
    for (const auto& entry : kRequestHeaderEntries)
      entries.emplace_back(entry.first, entry.second);
    return HeaderMapType(entries.begin(), entries.end());
  }());

  DCHECK(IsStringLowerCaseASCII(header));
  auto it = kHeaderMap->find(header);
  RequestHeaderType type =
      it != kHeaderMap->end() ? it->second : RequestHeaderType::kOther;
  record_func(type);
}

void RecordResponseHeaderChanged(ResponseHeaderType type) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.ResponseHeaderChanged",
                            type);
}

void RecordResponseHeaderAdded(ResponseHeaderType type) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.ResponseHeaderAdded", type);
}

void RecordResponseHeaderRemoved(ResponseHeaderType type) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.ResponseHeaderRemoved",
                            type);
}

using ResponseHeaderEntry = std::pair<const char*, ResponseHeaderType>;
constexpr ResponseHeaderEntry kResponseHeaderEntries[] = {
    {"accept-patch", ResponseHeaderType::kAcceptPatch},
    {"accept-ranges", ResponseHeaderType::kAcceptRanges},
    {"access-control-allow-credentials",
     ResponseHeaderType::kAccessControlAllowCredentials},
    {"access-control-allow-headers",
     ResponseHeaderType::kAccessControlAllowHeaders},
    {"access-control-allow-methods",
     ResponseHeaderType::kAccessControlAllowMethods},
    {"access-control-allow-origin",
     ResponseHeaderType::kAccessControlAllowOrigin},
    {"access-control-expose-headers",
     ResponseHeaderType::kAccessControlExposeHeaders},
    {"access-control-max-age", ResponseHeaderType::kAccessControlMaxAge},
    {"age", ResponseHeaderType::kAge},
    {"allow", ResponseHeaderType::kAllow},
    {"alt-svc", ResponseHeaderType::kAltSvc},
    {"cache-control", ResponseHeaderType::kCacheControl},
    {"clear-site-data", ResponseHeaderType::kClearSiteData},
    {"connection", ResponseHeaderType::kConnection},
    {"content-disposition", ResponseHeaderType::kContentDisposition},
    {"content-encoding", ResponseHeaderType::kContentEncoding},
    {"content-language", ResponseHeaderType::kContentLanguage},
    {"content-length", ResponseHeaderType::kContentLength},
    {"content-location", ResponseHeaderType::kContentLocation},
    {"content-range", ResponseHeaderType::kContentRange},
    {"content-security-policy", ResponseHeaderType::kContentSecurityPolicy},
    {"content-security-policy-report-only",
     ResponseHeaderType::kContentSecurityPolicyReportOnly},
    {"content-type", ResponseHeaderType::kContentType},
    {"date", ResponseHeaderType::kDate},
    {"etag", ResponseHeaderType::kETag},
    {"expect-ct", ResponseHeaderType::kExpectCT},
    {"expires", ResponseHeaderType::kExpires},
    {"feature-policy", ResponseHeaderType::kFeaturePolicy},
    {"keep-alive", ResponseHeaderType::kKeepAlive},
    {"large-allocation", ResponseHeaderType::kLargeAllocation},
    {"last-modified", ResponseHeaderType::kLastModified},
    {"location", ResponseHeaderType::kLocation},
    {"pragma", ResponseHeaderType::kPragma},
    {"proxy-authenticate", ResponseHeaderType::kProxyAuthenticate},
    {"proxy-connection", ResponseHeaderType::kProxyConnection},
    {"public-key-pins", ResponseHeaderType::kPublicKeyPins},
    {"public-key-pins-report-only",
     ResponseHeaderType::kPublicKeyPinsReportOnly},
    {"referrer-policy", ResponseHeaderType::kReferrerPolicy},
    {"refresh", ResponseHeaderType::kRefresh},
    {"retry-after", ResponseHeaderType::kRetryAfter},
    {"sec-websocket-accept", ResponseHeaderType::kSecWebSocketAccept},
    {"server", ResponseHeaderType::kServer},
    {"server-timing", ResponseHeaderType::kServerTiming},
    {"set-cookie", ResponseHeaderType::kSetCookie},
    {"sourcemap", ResponseHeaderType::kSourceMap},
    {"strict-transport-security", ResponseHeaderType::kStrictTransportSecurity},
    {"timing-allow-origin", ResponseHeaderType::kTimingAllowOrigin},
    {"tk", ResponseHeaderType::kTk},
    {"trailer", ResponseHeaderType::kTrailer},
    {"transfer-encoding", ResponseHeaderType::kTransferEncoding},
    {"upgrade", ResponseHeaderType::kUpgrade},
    {"vary", ResponseHeaderType::kVary},
    {"via", ResponseHeaderType::kVia},
    {"warning", ResponseHeaderType::kWarning},
    {"www-authenticate", ResponseHeaderType::kWWWAuthenticate},
    {"x-content-type-options", ResponseHeaderType::kXContentTypeOptions},
    {"x-dns-prefetch-control", ResponseHeaderType::kXDNSPrefetchControl},
    {"x-frame-options", ResponseHeaderType::kXFrameOptions},
    {"x-xss-protection", ResponseHeaderType::kXXSSProtection},
};

void RecordResponseHeader(base::StringPiece header,
                          void (*record_func)(ResponseHeaderType)) {
  using HeaderMapType = base::flat_map<base::StringPiece, ResponseHeaderType>;
  static const base::NoDestructor<HeaderMapType> kHeaderMap([] {
    std::vector<std::pair<base::StringPiece, ResponseHeaderType>> entries;
    entries.reserve(base::size(kResponseHeaderEntries));
    for (const auto& entry : kResponseHeaderEntries)
      entries.emplace_back(entry.first, entry.second);
    return HeaderMapType(entries.begin(), entries.end());
  }());

  DCHECK(IsStringLowerCaseASCII(header));
  auto it = kHeaderMap->find(header);
  ResponseHeaderType type =
      it != kHeaderMap->end() ? it->second : ResponseHeaderType::kOther;
  record_func(type);
}

// All entries other than kOther and kNone are mapped.
static_assert(static_cast<size_t>(ResponseHeaderType::kMaxValue) - 1 ==
                  base::size(kResponseHeaderEntries),
              "Invalid number of response header entries");

static_assert(ValidateHeaderEntries(kResponseHeaderEntries),
              "Invalid response header entries");

bool HasMatchingRemovedDNRRequestHeader(
    const extensions::WebRequestInfo& request,
    const std::string& header) {
  for (const auto& action : *request.dnr_actions) {
    if (std::find_if(action.request_headers_to_remove.begin(),
                     action.request_headers_to_remove.end(),
                     [&header](const char* header_to_remove) {
                       return base::EqualsCaseInsensitiveASCII(header_to_remove,
                                                               header);
                     }) != action.request_headers_to_remove.end()) {
      return true;
    }
  }

  return false;
}

bool HasMatchingRemovedDNRResponseHeader(
    const extensions::WebRequestInfo& request,
    const std::string& header) {
  for (const auto& action : *request.dnr_actions) {
    if (std::find_if(action.response_headers_to_remove.begin(),
                     action.response_headers_to_remove.end(),
                     [&header](const char* header_to_remove) {
                       return base::EqualsCaseInsensitiveASCII(
                           header, header_to_remove);
                     }) != action.response_headers_to_remove.end()) {
      return true;
    }
  }

  return false;
}

}  // namespace

IgnoredAction::IgnoredAction(extensions::ExtensionId extension_id,
                             web_request::IgnoredActionType action_type)
    : extension_id(std::move(extension_id)), action_type(action_type) {}

IgnoredAction::IgnoredAction(IgnoredAction&& rhs) = default;

bool ExtraInfoSpec::InitFromValue(content::BrowserContext* browser_context,
                                  const base::ListValue& value,
                                  int* extra_info_spec) {
  *extra_info_spec =
      extensions::ExtensionsBrowserClient::Get()
              ->ShouldForceWebRequestExtraHeaders(browser_context)
          ? EXTRA_HEADERS
          : 0;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    std::string str;
    if (!value.GetString(i, &str))
      return false;

    if (str == "requestHeaders")
      *extra_info_spec |= REQUEST_HEADERS;
    else if (str == "responseHeaders")
      *extra_info_spec |= RESPONSE_HEADERS;
    else if (str == "blocking")
      *extra_info_spec |= BLOCKING;
    else if (str == "asyncBlocking")
      *extra_info_spec |= ASYNC_BLOCKING;
    else if (str == "requestBody")
      *extra_info_spec |= REQUEST_BODY;
    else if (str == "extraHeaders")
      *extra_info_spec |= EXTRA_HEADERS;
    else
      return false;
  }
  // BLOCKING and ASYNC_BLOCKING are mutually exclusive.
  if ((*extra_info_spec & BLOCKING) && (*extra_info_spec & ASYNC_BLOCKING))
    return false;
  return true;
}

RequestCookie::RequestCookie() = default;
RequestCookie::RequestCookie(RequestCookie&& other) = default;
RequestCookie& RequestCookie ::operator=(RequestCookie&& other) = default;
RequestCookie::~RequestCookie() = default;

bool RequestCookie::operator==(const RequestCookie& other) const {
  return std::tie(name, value) == std::tie(other.name, other.value);
}

RequestCookie RequestCookie::Clone() const {
  RequestCookie clone;
  clone.name = name;
  clone.value = value;
  return clone;
}

ResponseCookie::ResponseCookie() = default;
ResponseCookie::ResponseCookie(ResponseCookie&& other) = default;
ResponseCookie& ResponseCookie ::operator=(ResponseCookie&& other) = default;
ResponseCookie::~ResponseCookie() = default;

bool ResponseCookie::operator==(const ResponseCookie& other) const {
  return std::tie(name, value, expires, max_age, domain, path, secure,
                  http_only) ==
         std::tie(other.name, other.value, other.expires, other.max_age,
                  other.domain, other.path, other.secure, other.http_only);
}

ResponseCookie ResponseCookie::Clone() const {
  ResponseCookie clone;
  clone.name = name;
  clone.value = value;
  clone.expires = expires;
  clone.max_age = max_age;
  clone.domain = domain;
  clone.path = path;
  clone.secure = secure;
  clone.http_only = http_only;
  return clone;
}

FilterResponseCookie::FilterResponseCookie() = default;
FilterResponseCookie::FilterResponseCookie(FilterResponseCookie&& other) =
    default;
FilterResponseCookie& FilterResponseCookie ::operator=(
    FilterResponseCookie&& other) = default;
FilterResponseCookie::~FilterResponseCookie() = default;

bool FilterResponseCookie::operator==(const FilterResponseCookie& other) const {
  // This ignores all of the fields of the base class ResponseCookie. Why?
  // https://crbug.com/916248
  return std::tie(age_lower_bound, age_upper_bound, session_cookie) ==
         std::tie(other.age_lower_bound, other.age_upper_bound,
                  other.session_cookie);
}

FilterResponseCookie FilterResponseCookie::Clone() const {
  FilterResponseCookie clone;
  clone.name = name;
  clone.value = value;
  clone.expires = expires;
  clone.max_age = max_age;
  clone.domain = domain;
  clone.path = path;
  clone.secure = secure;
  clone.http_only = http_only;
  clone.age_upper_bound = age_upper_bound;
  clone.age_lower_bound = age_lower_bound;
  clone.session_cookie = session_cookie;
  return clone;
}

RequestCookieModification::RequestCookieModification() = default;
RequestCookieModification::RequestCookieModification(
    RequestCookieModification&& other) = default;
RequestCookieModification& RequestCookieModification ::operator=(
    RequestCookieModification&& other) = default;
RequestCookieModification::~RequestCookieModification() = default;

bool RequestCookieModification::operator==(
    const RequestCookieModification& other) const {
  // This ignores |type|. Why? https://crbug.com/916248
  return std::tie(filter, modification) ==
         std::tie(other.filter, other.modification);
}

RequestCookieModification RequestCookieModification::Clone() const {
  RequestCookieModification clone;
  clone.type = type;
  if (filter.has_value())
    clone.filter = filter->Clone();
  if (modification.has_value())
    clone.modification = modification->Clone();
  return clone;
}

ResponseCookieModification::ResponseCookieModification() : type(ADD) {}
ResponseCookieModification::ResponseCookieModification(
    ResponseCookieModification&& other) = default;
ResponseCookieModification& ResponseCookieModification ::operator=(
    ResponseCookieModification&& other) = default;
ResponseCookieModification::~ResponseCookieModification() = default;

bool ResponseCookieModification::operator==(
    const ResponseCookieModification& other) const {
  // This ignores |type|. Why? https://crbug.com/916248
  return std::tie(filter, modification) ==
         std::tie(other.filter, other.modification);
}

ResponseCookieModification ResponseCookieModification::Clone() const {
  ResponseCookieModification clone;
  clone.type = type;
  if (filter.has_value())
    clone.filter = filter->Clone();
  if (modification.has_value())
    clone.modification = modification->Clone();
  return clone;
}

EventResponseDelta::EventResponseDelta(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

EventResponseDelta::EventResponseDelta(EventResponseDelta&& other) = default;
EventResponseDelta& EventResponseDelta ::operator=(EventResponseDelta&& other) =
    default;

EventResponseDelta::~EventResponseDelta() {
}

bool InDecreasingExtensionInstallationTimeOrder(const EventResponseDelta& a,
                                                const EventResponseDelta& b) {
  return a.extension_install_time > b.extension_install_time;
}

std::unique_ptr<base::ListValue> StringToCharList(const std::string& s) {
  auto result = std::make_unique<base::ListValue>();
  for (size_t i = 0, n = s.size(); i < n; ++i) {
    result->AppendInteger(*reinterpret_cast<const unsigned char*>(&s[i]));
  }
  return result;
}

bool CharListToString(const base::ListValue* list, std::string* out) {
  if (!list)
    return false;
  const size_t list_length = list->GetSize();
  out->resize(list_length);
  int value = 0;
  for (size_t i = 0; i < list_length; ++i) {
    if (!list->GetInteger(i, &value) || value < 0 || value > 255)
      return false;
    unsigned char tmp = static_cast<unsigned char>(value);
    (*out)[i] = *reinterpret_cast<char*>(&tmp);
  }
  return true;
}

EventResponseDelta CalculateOnBeforeRequestDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const GURL& new_url) {
  EventResponseDelta result(extension_id, extension_install_time);
  result.cancel = cancel;
  result.new_url = new_url;
  return result;
}

EventResponseDelta CalculateOnBeforeSendHeadersDelta(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    net::HttpRequestHeaders* old_headers,
    net::HttpRequestHeaders* new_headers,
    int extra_info_spec) {
  EventResponseDelta result(extension_id, extension_install_time);
  result.cancel = cancel;

  // The event listener might not have passed any new headers if it
  // just wanted to cancel the request.
  if (new_headers) {
    // Find deleted headers.
    {
      net::HttpRequestHeaders::Iterator i(*old_headers);
      while (i.GetNext()) {
        if (ShouldHideRequestHeader(browser_context, extra_info_spec,
                                    i.name())) {
          continue;
        }
        if (!new_headers->HasHeader(i.name())) {
          result.deleted_request_headers.push_back(i.name());
        }
      }
    }

    // Find modified headers.
    {
      net::HttpRequestHeaders::Iterator i(*new_headers);
      while (i.GetNext()) {
        if (ShouldHideRequestHeader(browser_context, extra_info_spec,
                                    i.name())) {
          continue;
        }
        std::string value;
        if (!old_headers->GetHeader(i.name(), &value) || i.value() != value) {
          result.modified_request_headers.SetHeader(i.name(), i.value());
        }
      }
    }
  }
  return result;
}

EventResponseDelta CalculateOnHeadersReceivedDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const GURL& old_url,
    const GURL& new_url,
    const net::HttpResponseHeaders* old_response_headers,
    ResponseHeaders* new_response_headers,
    int extra_info_spec) {
  EventResponseDelta result(extension_id, extension_install_time);
  result.cancel = cancel;
  result.new_url = new_url;

  if (!new_response_headers)
    return result;

  extensions::ExtensionsAPIClient* api_client =
      extensions::ExtensionsAPIClient::Get();

  // Find deleted headers (header keys are treated case insensitively).
  {
    size_t iter = 0;
    std::string name;
    std::string value;
    while (old_response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
      if (api_client->ShouldHideResponseHeader(old_url, name))
        continue;
      if (ShouldHideResponseHeader(extra_info_spec, name))
        continue;
      std::string name_lowercase = base::ToLowerASCII(name);
      bool header_found = false;
      for (const auto& i : *new_response_headers) {
        if (base::LowerCaseEqualsASCII(i.first, name_lowercase) &&
            value == i.second) {
          header_found = true;
          break;
        }
      }
      if (!header_found)
        result.deleted_response_headers.push_back(ResponseHeader(name, value));
    }
  }

  // Find added headers (header keys are treated case insensitively).
  {
    for (const auto& i : *new_response_headers) {
      if (api_client->ShouldHideResponseHeader(old_url, i.first))
        continue;
      if (ShouldHideResponseHeader(extra_info_spec, i.first))
        continue;
      std::string name_lowercase = base::ToLowerASCII(i.first);
      size_t iter = 0;
      std::string name;
      std::string value;
      bool header_found = false;
      while (old_response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
        if (base::LowerCaseEqualsASCII(name, name_lowercase) &&
            value == i.second) {
          header_found = true;
          break;
        }
      }
      if (!header_found)
        result.added_response_headers.push_back(i);
    }
  }

  return result;
}

EventResponseDelta CalculateOnAuthRequiredDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    base::Optional<net::AuthCredentials> auth_credentials) {
  EventResponseDelta result(extension_id, extension_install_time);
  result.cancel = cancel;
  result.auth_credentials = std::move(auth_credentials);
  return result;
}

void MergeCancelOfResponses(const EventResponseDeltas& deltas, bool* canceled) {
  for (const auto& delta : deltas) {
    if (delta.cancel) {
      *canceled = true;
      break;
    }
  }
}

// Helper function for MergeRedirectUrlOfResponses() that allows ignoring
// all redirects but those to data:// urls and about:blank. This is important
// to treat these URLs as "cancel urls", i.e. URLs that extensions redirect
// to if they want to express that they want to cancel a request. This reduces
// the number of conflicts that we need to flag, as canceling is considered
// a higher precedence operation that redirects.
// Returns whether a redirect occurred.
static bool MergeRedirectUrlOfResponsesHelper(
    const GURL& url,
    const EventResponseDeltas& deltas,
    GURL* new_url,
    IgnoredActions* ignored_actions,
    bool consider_only_cancel_scheme_urls) {
  // Redirecting WebSocket handshake request is prohibited.
  if (url.SchemeIsWSOrWSS())
    return false;

  bool redirected = false;

  for (const auto& delta : deltas) {
    if (delta.new_url.is_empty())
      continue;
    if (consider_only_cancel_scheme_urls &&
        !delta.new_url.SchemeIs(url::kDataScheme) &&
        delta.new_url.spec() != "about:blank") {
      continue;
    }

    if (!redirected || *new_url == delta.new_url) {
      *new_url = delta.new_url;
      redirected = true;
    } else {
      ignored_actions->emplace_back(delta.extension_id,
                                    web_request::IGNORED_ACTION_TYPE_REDIRECT);
    }
  }
  return redirected;
}

void MergeRedirectUrlOfResponses(const GURL& url,
                                 const EventResponseDeltas& deltas,
                                 GURL* new_url,
                                 IgnoredActions* ignored_actions) {
  // First handle only redirects to data:// URLs and about:blank. These are a
  // special case as they represent a way of cancelling a request.
  if (MergeRedirectUrlOfResponsesHelper(url, deltas, new_url, ignored_actions,
                                        true)) {
    // If any extension cancelled a request by redirecting to a data:// URL or
    // about:blank, we don't consider the other redirects.
    return;
  }

  // Handle all other redirects.
  MergeRedirectUrlOfResponsesHelper(url, deltas, new_url, ignored_actions,
                                    false);
}

void MergeOnBeforeRequestResponses(const GURL& url,
                                   const EventResponseDeltas& deltas,
                                   GURL* new_url,
                                   IgnoredActions* ignored_actions) {
  MergeRedirectUrlOfResponses(url, deltas, new_url, ignored_actions);
}

static bool DoesRequestCookieMatchFilter(
    const ParsedRequestCookie& cookie,
    const base::Optional<RequestCookie>& filter) {
  if (!filter.has_value())
    return true;
  if (filter->name.has_value() && cookie.first != *filter->name)
    return false;
  if (filter->value.has_value() && cookie.second != *filter->value)
    return false;
  return true;
}

// Applies all CookieModificationType::ADD operations for request cookies of
// |deltas| to |cookies|. Returns whether any cookie was added.
static bool MergeAddRequestCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedRequestCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (const auto& delta : base::Reversed(deltas)) {
    const RequestCookieModifications& modifications =
        delta.request_cookie_modifications;
    for (auto mod = modifications.cbegin(); mod != modifications.cend();
         ++mod) {
      if (mod->type != ADD || !mod->modification.has_value())
        continue;

      if (!mod->modification->name.has_value() ||
          !mod->modification->value.has_value())
        continue;

      const std::string& new_name = *mod->modification->name;
      const std::string& new_value = *mod->modification->value;

      bool cookie_with_same_name_found = false;
      for (auto cookie = cookies->begin();
           cookie != cookies->end() && !cookie_with_same_name_found; ++cookie) {
        if (cookie->first == new_name) {
          if (cookie->second != new_value) {
            cookie->second = new_value;
            modified = true;
          }
          cookie_with_same_name_found = true;
        }
      }
      if (!cookie_with_same_name_found) {
        cookies->emplace_back(new_name, new_value);
        modified = true;
      }
    }
  }
  return modified;
}

// Applies all CookieModificationType::EDIT operations for request cookies of
// |deltas| to |cookies|. Returns whether any cookie was modified.
static bool MergeEditRequestCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedRequestCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (const auto& delta : base::Reversed(deltas)) {
    const RequestCookieModifications& modifications =
        delta.request_cookie_modifications;
    for (auto mod = modifications.cbegin(); mod != modifications.cend();
         ++mod) {
      if (mod->type != EDIT || !mod->modification.has_value())
        continue;

      if (!mod->modification->value.has_value())
        continue;

      const std::string& new_value = *mod->modification->value;
      const base::Optional<RequestCookie>& filter = mod->filter;
      for (auto cookie = cookies->begin(); cookie != cookies->end(); ++cookie) {
        if (!DoesRequestCookieMatchFilter(*cookie, filter))
          continue;
        // If the edit operation tries to modify the cookie name, we just ignore
        // this. We only modify the cookie value.
        if (cookie->second != new_value) {
          cookie->second = new_value;
          modified = true;
        }
      }
    }
  }
  return modified;
}

// Applies all CookieModificationType::REMOVE operations for request cookies of
// |deltas| to |cookies|. Returns whether any cookie was deleted.
static bool MergeRemoveRequestCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedRequestCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (const auto& delta : base::Reversed(deltas)) {
    const RequestCookieModifications& modifications =
        delta.request_cookie_modifications;
    for (auto mod = modifications.cbegin(); mod != modifications.cend();
         ++mod) {
      if (mod->type != REMOVE)
        continue;

      const base::Optional<RequestCookie>& filter = mod->filter;
      auto i = cookies->begin();
      while (i != cookies->end()) {
        if (DoesRequestCookieMatchFilter(*i, filter)) {
          i = cookies->erase(i);
          modified = true;
        } else {
          ++i;
        }
      }
    }
  }
  return modified;
}

void MergeCookiesInOnBeforeSendHeadersResponses(
    const GURL& url,
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers) {
  // Skip all work if there are no registered cookie modifications.
  bool cookie_modifications_exist = false;
  for (const auto& delta : deltas) {
    cookie_modifications_exist |= !delta.request_cookie_modifications.empty();
  }
  if (!cookie_modifications_exist)
    return;

  // Parse old cookie line.
  std::string cookie_header;
  request_headers->GetHeader(net::HttpRequestHeaders::kCookie, &cookie_header);
  ParsedRequestCookies cookies;
  net::cookie_util::ParseRequestCookieLine(cookie_header, &cookies);

  // Modify cookies.
  bool modified = false;
  modified |= MergeAddRequestCookieModifications(deltas, &cookies);
  modified |= MergeEditRequestCookieModifications(deltas, &cookies);
  modified |= MergeRemoveRequestCookieModifications(deltas, &cookies);

  // Reassemble and store new cookie line.
  if (modified) {
    std::string new_cookie_header =
        net::cookie_util::SerializeRequestCookieLine(cookies);
    request_headers->SetHeader(net::HttpRequestHeaders::kCookie,
                               new_cookie_header);
  }
}

void MergeOnBeforeSendHeadersResponses(
    const extensions::WebRequestInfo& request,
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers,
    IgnoredActions* ignored_actions,
    std::set<std::string>* removed_headers,
    std::set<std::string>* set_headers,
    bool* request_headers_modified) {
  DCHECK(request_headers_modified);
  DCHECK(removed_headers->empty());
  DCHECK(set_headers->empty());
  *request_headers_modified = false;

  // Exhaustive subsets of |set_headers|. Split into a set for added headers and
  // a set for overridden headers.
  std::set<std::string> overridden_headers;
  std::set<std::string> added_headers;

  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (const auto& delta : deltas) {
    if (delta.modified_request_headers.IsEmpty() &&
        delta.deleted_request_headers.empty()) {
      continue;
    }

    // Check whether any modification affects a request header that
    // has been modified differently before. As deltas is sorted by decreasing
    // extension installation order, this takes care of precedence.
    bool extension_conflicts = false;
    {
      net::HttpRequestHeaders::Iterator modification(
          delta.modified_request_headers);
      while (modification.GetNext() && !extension_conflicts) {
        // This modification sets |key| to |value|.
        const std::string key = base::ToLowerASCII(modification.name());
        const std::string& value = modification.value();

        // We must not modify anything that has been deleted before.
        if (base::Contains(*removed_headers, key)) {
          extension_conflicts = true;
          break;
        }

        // Prevent extensions from adding any header removed by the Declarative
        // Net Request API.
        DCHECK(request.dnr_actions);
        if (HasMatchingRemovedDNRRequestHeader(request, key)) {
          extension_conflicts = true;
          break;
        }

        // We must not modify anything that has been set to a *different*
        // value before.
        if (base::Contains(*set_headers, key)) {
          std::string current_value;
          if (!request_headers->GetHeader(key, &current_value) ||
              current_value != value) {
            extension_conflicts = true;
            break;
          }
        }
      }
    }

    // Check whether any deletion affects a request header that has been
    // modified before.
    {
      for (const std::string& key : delta.deleted_request_headers) {
        if (base::Contains(*set_headers, base::ToLowerASCII(key))) {
          extension_conflicts = true;
          break;
        }
      }
    }

    // Now execute the modifications if there were no conflicts.
    if (!extension_conflicts) {
      // Populate |set_headers|, |overridden_headers| and |added_headers| and
      // perform the modifications.
      net::HttpRequestHeaders::Iterator modification(
          delta.modified_request_headers);
      while (modification.GetNext()) {
        std::string key = base::ToLowerASCII(modification.name());
        if (!request_headers->HasHeader(key)) {
          added_headers.insert(key);
        } else if (!base::Contains(added_headers, key)) {
          // Note: |key| will only be present in |added_headers| if this is an
          // identical edit.
          overridden_headers.insert(key);
        }

        set_headers->insert(key);

        request_headers->SetHeader(key, modification.value());
      }

      // Perform all deletions and record which keys were deleted.
      {
        for (const auto& header : delta.deleted_request_headers) {
          request_headers->RemoveHeader(header);
          removed_headers->insert(base::ToLowerASCII(header));
        }
      }
      *request_headers_modified = true;
    } else {
      ignored_actions->emplace_back(
          delta.extension_id, web_request::IGNORED_ACTION_TYPE_REQUEST_HEADERS);
    }
  }

  auto record_request_headers = [](const std::set<std::string>& headers,
                                   void (*record_func)(RequestHeaderType)) {
    if (headers.empty()) {
      record_func(RequestHeaderType::kNone);
      return;
    }
    for (const auto& header : headers)
      RecordRequestHeader(header, record_func);
  };

  // Some sanity checks.
  DCHECK(std::all_of(removed_headers->begin(), removed_headers->end(),
                     IsStringLowerCaseASCII));
  DCHECK(std::all_of(set_headers->begin(), set_headers->end(),
                     IsStringLowerCaseASCII));
  DCHECK(std::all_of(overridden_headers.begin(), overridden_headers.end(),
                     IsStringLowerCaseASCII));
  DCHECK(std::all_of(added_headers.begin(), added_headers.end(),
                     IsStringLowerCaseASCII));
  DCHECK(*set_headers == base::STLSetUnion<std::set<std::string>>(
                             added_headers, overridden_headers));
  DCHECK(base::STLSetIntersection<std::set<std::string>>(added_headers,
                                                         overridden_headers)
             .empty());
  DCHECK(base::STLSetIntersection<std::set<std::string>>(*removed_headers,
                                                         *set_headers)
             .empty());

  // Record request header removals, additions and modifications.
  record_request_headers(*removed_headers, &RecordRequestHeaderRemoved);
  record_request_headers(added_headers, &RecordRequestHeaderAdded);
  record_request_headers(overridden_headers, &RecordRequestHeaderChanged);

  // Currently, conflicts are ignored while merging cookies.
  MergeCookiesInOnBeforeSendHeadersResponses(request.url, deltas,
                                             request_headers);
}

// Retrieves all cookies from |override_response_headers|.
static ParsedResponseCookies GetResponseCookies(
    scoped_refptr<net::HttpResponseHeaders> override_response_headers) {
  ParsedResponseCookies result;

  size_t iter = 0;
  std::string value;
  while (override_response_headers->EnumerateHeader(&iter, "Set-Cookie",
                                                    &value)) {
    result.push_back(std::make_unique<net::ParsedCookie>(value));
  }
  return result;
}

// Stores all |cookies| in |override_response_headers| deleting previously
// existing cookie definitions.
static void StoreResponseCookies(
    const ParsedResponseCookies& cookies,
    scoped_refptr<net::HttpResponseHeaders> override_response_headers) {
  override_response_headers->RemoveHeader("Set-Cookie");
  for (const std::unique_ptr<net::ParsedCookie>& cookie : cookies) {
    override_response_headers->AddHeader("Set-Cookie: " +
                                         cookie->ToCookieLine());
  }
}

// Modifies |cookie| according to |modification|. Each value that is set in
// |modification| is applied to |cookie|.
static bool ApplyResponseCookieModification(const ResponseCookie& modification,
                                            net::ParsedCookie* cookie) {
  bool modified = false;
  if (modification.name.has_value())
    modified |= cookie->SetName(*modification.name);
  if (modification.value.has_value())
    modified |= cookie->SetValue(*modification.value);
  if (modification.expires.has_value())
    modified |= cookie->SetExpires(*modification.expires);
  if (modification.max_age.has_value())
    modified |= cookie->SetMaxAge(base::NumberToString(*modification.max_age));
  if (modification.domain.has_value())
    modified |= cookie->SetDomain(*modification.domain);
  if (modification.path.has_value())
    modified |= cookie->SetPath(*modification.path);
  if (modification.secure.has_value())
    modified |= cookie->SetIsSecure(*modification.secure);
  if (modification.http_only.has_value())
    modified |= cookie->SetIsHttpOnly(*modification.http_only);
  return modified;
}

static bool DoesResponseCookieMatchFilter(
    const net::ParsedCookie& cookie,
    const base::Optional<FilterResponseCookie>& filter) {
  if (!cookie.IsValid())
    return false;
  if (!filter.has_value())
    return true;
  if (filter->name && cookie.Name() != *filter->name)
    return false;
  if (filter->value && cookie.Value() != *filter->value)
    return false;
  if (filter->expires) {
    std::string actual_value =
        cookie.HasExpires() ? cookie.Expires() : std::string();
    if (actual_value != *filter->expires)
      return false;
  }
  if (filter->max_age) {
    std::string actual_value =
        cookie.HasMaxAge() ? cookie.MaxAge() : std::string();
    if (actual_value != base::NumberToString(*filter->max_age))
      return false;
  }
  if (filter->domain) {
    std::string actual_value =
        cookie.HasDomain() ? cookie.Domain() : std::string();
    if (actual_value != *filter->domain)
      return false;
  }
  if (filter->path) {
    std::string actual_value = cookie.HasPath() ? cookie.Path() : std::string();
    if (actual_value != *filter->path)
      return false;
  }
  if (filter->secure && cookie.IsSecure() != *filter->secure)
    return false;
  if (filter->http_only && cookie.IsHttpOnly() != *filter->http_only)
    return false;
  if (filter->age_upper_bound || filter->age_lower_bound ||
      (filter->session_cookie && *filter->session_cookie)) {
    int64_t seconds_to_expiry;
    bool lifetime_parsed = ParseCookieLifetime(cookie, &seconds_to_expiry);
    if (filter->age_upper_bound && seconds_to_expiry > *filter->age_upper_bound)
      return false;
    if (filter->age_lower_bound && seconds_to_expiry < *filter->age_lower_bound)
      return false;
    if (filter->session_cookie && *filter->session_cookie && lifetime_parsed)
      return false;
  }
  return true;
}

// Applies all CookieModificationType::ADD operations for response cookies of
// |deltas| to |cookies|. Returns whether any cookie was added.
static bool MergeAddResponseCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedResponseCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (const auto& delta : base::Reversed(deltas)) {
    const ResponseCookieModifications& modifications =
        delta.response_cookie_modifications;
    for (const auto& mod : modifications) {
      if (mod.type != ADD || !mod.modification.has_value())
        continue;

      // Cookie names are not unique in response cookies so we always append
      // and never override.
      auto cookie = std::make_unique<net::ParsedCookie>(std::string());
      ApplyResponseCookieModification(mod.modification.value(), cookie.get());
      cookies->push_back(std::move(cookie));
      modified = true;
    }
  }
  return modified;
}

// Applies all CookieModificationType::EDIT operations for response cookies of
// |deltas| to |cookies|. Returns whether any cookie was modified.
static bool MergeEditResponseCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedResponseCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (const auto& delta : base::Reversed(deltas)) {
    const ResponseCookieModifications& modifications =
        delta.response_cookie_modifications;
    for (const auto& mod : modifications) {
      if (mod.type != EDIT || !mod.modification.has_value())
        continue;

      for (const std::unique_ptr<net::ParsedCookie>& cookie : *cookies) {
        if (DoesResponseCookieMatchFilter(*cookie.get(), mod.filter)) {
          modified |= ApplyResponseCookieModification(mod.modification.value(),
                                                      cookie.get());
        }
      }
    }
  }
  return modified;
}

// Applies all CookieModificationType::REMOVE operations for response cookies of
// |deltas| to |cookies|. Returns whether any cookie was deleted.
static bool MergeRemoveResponseCookieModifications(
    const EventResponseDeltas& deltas,
    ParsedResponseCookies* cookies) {
  bool modified = false;
  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (const auto& delta : base::Reversed(deltas)) {
    const ResponseCookieModifications& modifications =
        delta.response_cookie_modifications;
    for (auto mod = modifications.cbegin(); mod != modifications.cend();
         ++mod) {
      if (mod->type != REMOVE)
        continue;

      auto i = cookies->begin();
      while (i != cookies->end()) {
        if (DoesResponseCookieMatchFilter(*i->get(), mod->filter)) {
          i = cookies->erase(i);
          modified = true;
        } else {
          ++i;
        }
      }
    }
  }
  return modified;
}

void MergeCookiesInOnHeadersReceivedResponses(
    const GURL& url,
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
  // Skip all work if there are no registered cookie modifications.
  bool cookie_modifications_exist = false;
  for (const auto& delta : base::Reversed(deltas)) {
    cookie_modifications_exist |= !delta.response_cookie_modifications.empty();
  }

  if (!cookie_modifications_exist)
    return;

  // Only create a copy if we really want to modify the response headers.
  if (override_response_headers->get() == NULL) {
    *override_response_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        original_response_headers->raw_headers());
  }

  ParsedResponseCookies cookies =
      GetResponseCookies(*override_response_headers);

  bool modified = false;
  modified |= MergeAddResponseCookieModifications(deltas, &cookies);
  modified |= MergeEditResponseCookieModifications(deltas, &cookies);
  modified |= MergeRemoveResponseCookieModifications(deltas, &cookies);

  // Store new value.
  if (modified)
    StoreResponseCookies(cookies, *override_response_headers);
}

// Converts the key of the (key, value) pair to lower case.
static ResponseHeader ToLowerCase(const ResponseHeader& header) {
  return ResponseHeader(base::ToLowerASCII(header.first), header.second);
}

void MergeOnHeadersReceivedResponses(
    const extensions::WebRequestInfo& request,
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* preserve_fragment_on_redirect_url,
    IgnoredActions* ignored_actions,
    bool* response_headers_modified) {
  DCHECK(response_headers_modified);
  *response_headers_modified = false;

  // Here we collect which headers we have removed or added so far due to
  // extensions of higher precedence. Header keys are always stored as
  // lower case.
  std::set<ResponseHeader> removed_headers;
  std::set<ResponseHeader> added_headers;

  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (const auto& delta : deltas) {
    if (delta.added_response_headers.empty() &&
        delta.deleted_response_headers.empty()) {
      continue;
    }

    // Only create a copy if we really want to modify the response headers.
    if (override_response_headers->get() == nullptr) {
      *override_response_headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(
              original_response_headers->raw_headers());
    }

    // We consider modifications as pairs of (delete, add) operations.
    // If a header is deleted twice by different extensions we assume that the
    // intention was to modify it to different values and consider this a
    // conflict. As deltas is sorted by decreasing extension installation order,
    // this takes care of precedence.
    bool extension_conflicts = false;
    for (const ResponseHeader& header : delta.deleted_response_headers) {
      if (removed_headers.find(ToLowerCase(header)) != removed_headers.end()) {
        extension_conflicts = true;
        break;
      }
    }

    // Prevent extensions from adding any response header which was removed by
    // the Declarative Net Request API.
    DCHECK(request.dnr_actions);
    if (!extension_conflicts) {
      for (const ResponseHeader& header : delta.added_response_headers) {
        if (HasMatchingRemovedDNRResponseHeader(request, header.first)) {
          extension_conflicts = true;
          break;
        }
      }
    }

    // Now execute the modifications if there were no conflicts.
    if (!extension_conflicts) {
      // Delete headers
      {
        for (const ResponseHeader& header : delta.deleted_response_headers) {
          (*override_response_headers)
              ->RemoveHeaderLine(header.first, header.second);
          removed_headers.insert(ToLowerCase(header));
        }
      }

      // Add headers.
      {
        for (const ResponseHeader& header : delta.added_response_headers) {
          ResponseHeader lowercase_header(ToLowerCase(header));
          if (added_headers.find(lowercase_header) != added_headers.end())
            continue;
          added_headers.insert(lowercase_header);
          (*override_response_headers)
              ->AddHeader(header.first + ": " + header.second);
        }
      }
      *response_headers_modified = true;
    } else {
      ignored_actions->emplace_back(
          delta.extension_id,
          web_request::IGNORED_ACTION_TYPE_RESPONSE_HEADERS);
    }
  }

  // Currently, conflicts are ignored while merging cookies.
  MergeCookiesInOnHeadersReceivedResponses(request.url, deltas,
                                           original_response_headers,
                                           override_response_headers);

  GURL new_url;
  MergeRedirectUrlOfResponses(request.url, deltas, &new_url, ignored_actions);
  if (new_url.is_valid()) {
    // Only create a copy if we really want to modify the response headers.
    if (override_response_headers->get() == NULL) {
      *override_response_headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(
              original_response_headers->raw_headers());
    }
    (*override_response_headers)->ReplaceStatusLine("HTTP/1.1 302 Found");
    (*override_response_headers)->RemoveHeader("location");
    (*override_response_headers)->AddHeader("Location: " + new_url.spec());
    // Prevent the original URL's fragment from being added to the new URL.
    *preserve_fragment_on_redirect_url = new_url;
  }

  // Record metrics.
  {
    auto record_response_headers =
        [](const std::set<base::StringPiece>& headers,
           void (*record_func)(ResponseHeaderType)) {
          if (headers.empty()) {
            record_func(ResponseHeaderType::kNone);
            return;
          }

          for (const auto& header : headers)
            RecordResponseHeader(header, record_func);
        };

    std::set<base::StringPiece> modified_header_names;
    std::set<base::StringPiece> added_header_names;
    std::set<base::StringPiece> removed_header_names;
    for (const ResponseHeader& header : added_headers) {
      // Skip logging this header if this was subsequently removed by an
      // extension.
      if (!override_response_headers->get()->HasHeader(header.first))
        continue;

      if (original_response_headers->HasHeader(header.first))
        modified_header_names.insert(header.first);
      else
        added_header_names.insert(header.first);
    }

    for (const ResponseHeader& header : removed_headers) {
      if (!override_response_headers->get()->HasHeader(header.first))
        removed_header_names.insert(header.first);
      else
        modified_header_names.insert(header.first);
    }

    DCHECK(std::all_of(modified_header_names.begin(),
                       modified_header_names.end(), IsStringLowerCaseASCII));
    DCHECK(std::all_of(added_header_names.begin(), added_header_names.end(),
                       IsStringLowerCaseASCII));
    DCHECK(std::all_of(removed_header_names.begin(), removed_header_names.end(),
                       IsStringLowerCaseASCII));

    record_response_headers(modified_header_names,
                            &RecordResponseHeaderChanged);
    record_response_headers(added_header_names, &RecordResponseHeaderAdded);
    record_response_headers(removed_header_names, &RecordResponseHeaderRemoved);
  }
}

bool MergeOnAuthRequiredResponses(const EventResponseDeltas& deltas,
                                  net::AuthCredentials* auth_credentials,
                                  IgnoredActions* ignored_actions) {
  CHECK(auth_credentials);
  bool credentials_set = false;

  for (const auto& delta : deltas) {
    if (!delta.auth_credentials.has_value())
      continue;
    bool different =
        auth_credentials->username() != delta.auth_credentials->username() ||
        auth_credentials->password() != delta.auth_credentials->password();
    if (credentials_set && different) {
      ignored_actions->emplace_back(
          delta.extension_id,
          web_request::IGNORED_ACTION_TYPE_AUTH_CREDENTIALS);
    } else {
      *auth_credentials = *delta.auth_credentials;
      credentials_set = true;
    }
  }
  return credentials_set;
}

void ClearCacheOnNavigation() {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    ClearCacheOnNavigationOnUI();
  } else {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&ClearCacheOnNavigationOnUI));
  }
}

// Converts the |name|, |value| pair of a http header to a HttpHeaders
// dictionary.
std::unique_ptr<base::DictionaryValue> CreateHeaderDictionary(
    const std::string& name,
    const std::string& value) {
  auto header = std::make_unique<base::DictionaryValue>();
  header->SetString(keys::kHeaderNameKey, name);
  if (base::IsStringUTF8(value)) {
    header->SetString(keys::kHeaderValueKey, value);
  } else {
    header->Set(keys::kHeaderBinaryValueKey,
                StringToCharList(value));
  }
  return header;
}

bool ShouldHideRequestHeader(content::BrowserContext* browser_context,
                             int extra_info_spec,
                             const std::string& name) {
  static const std::set<std::string> kRequestHeadersForOutOfBlinkCors =
      GetExtraHeaderRequestHeaders(/*is_out_of_blink_cors_enabled=*/true);
  static const std::set<std::string> kRequestHeadersForBlinkCors =
      GetExtraHeaderRequestHeaders(/*is_out_of_blink_cors_enabled=*/false);
  bool is_out_of_blink_cors_enabled =
      browser_context && browser_context->ShouldEnableOutOfBlinkCors();
  const std::set<std::string>& request_headers =
      is_out_of_blink_cors_enabled ? kRequestHeadersForOutOfBlinkCors
                                   : kRequestHeadersForBlinkCors;
  return !(extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS) &&
         request_headers.find(base::ToLowerASCII(name)) !=
             request_headers.end();
}

bool ShouldHideResponseHeader(int extra_info_spec, const std::string& name) {
  return !(extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS) &&
         base::LowerCaseEqualsASCII(name, "set-cookie");
}

}  // namespace extension_web_request_api_helpers
