// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper classes and functions used for the WebRequest API.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_HELPERS_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_HELPERS_H_

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/extension_id.h"
#include "net/base/auth.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace base {
class ListValue;
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
struct WebRequestInfo;
}

namespace extension_web_request_api_helpers {

using ResponseHeader = std::pair<std::string, std::string>;
using ResponseHeaders = std::vector<ResponseHeader>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RequestHeaderType {
  kNone = 0,
  kOther = 1,
  kAccept = 2,
  kAcceptCharset = 3,
  kAcceptEncoding = 4,
  kAcceptLanguage = 5,
  kAccessControlRequestHeaders = 6,
  kAccessControlRequestMethod = 7,
  kAuthorization = 8,
  kCacheControl = 9,
  kConnection = 10,
  kContentEncoding = 11,
  kContentLanguage = 12,
  kContentLength = 13,
  kContentLocation = 14,
  kContentType = 15,
  kCookie = 16,
  kDate = 17,
  kDnt = 18,
  kEarlyData = 19,
  kExpect = 20,
  kForwarded = 21,
  kFrom = 22,
  kHost = 23,
  kIfMatch = 24,
  kIfModifiedSince = 25,
  kIfNoneMatch = 26,
  kIfRange = 27,
  kIfUnmodifiedSince = 28,
  kKeepAlive = 29,
  kOrigin = 30,
  kPragma = 31,
  kProxyAuthorization = 32,
  kProxyConnection = 33,
  kRange = 34,
  kReferer = 35,
  kSecOriginPolicy = 36,
  kTe = 37,
  kTransferEncoding = 38,
  kUpgrade = 39,
  kUpgradeInsecureRequests = 40,
  kUserAgent = 41,
  kVia = 42,
  kWarning = 43,
  kXForwardedFor = 44,
  kXForwardedHost = 45,
  kXForwardedProto = 46,
  kMaxValue = kXForwardedProto,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ResponseHeaderType {
  kNone = 0,
  kOther = 1,
  kAcceptPatch = 2,
  kAcceptRanges = 3,
  kAccessControlAllowCredentials = 4,
  kAccessControlAllowHeaders = 5,
  kAccessControlAllowMethods = 6,
  kAccessControlAllowOrigin = 7,
  kAccessControlExposeHeaders = 8,
  kAccessControlMaxAge = 9,
  kAge = 10,
  kAllow = 11,
  kAltSvc = 12,
  kCacheControl = 13,
  kClearSiteData = 14,
  kConnection = 15,
  kContentDisposition = 16,
  kContentEncoding = 17,
  kContentLanguage = 18,
  kContentLength = 19,
  kContentLocation = 20,
  kContentRange = 21,
  kContentSecurityPolicy = 22,
  kContentSecurityPolicyReportOnly = 23,
  kContentType = 24,
  kDate = 25,
  kETag = 26,
  kExpectCT = 27,
  kExpires = 28,
  kFeaturePolicy = 29,
  kKeepAlive = 30,
  kLargeAllocation = 31,
  kLastModified = 32,
  kLocation = 33,
  kPragma = 34,
  kProxyAuthenticate = 35,
  kProxyConnection = 36,
  kPublicKeyPins = 37,
  kPublicKeyPinsReportOnly = 38,
  kReferrerPolicy = 39,
  kRefresh = 40,
  kRetryAfter = 41,
  kSecWebSocketAccept = 42,
  kServer = 43,
  kServerTiming = 44,
  kSetCookie = 45,
  kSourceMap = 46,
  kStrictTransportSecurity = 47,
  kTimingAllowOrigin = 48,
  kTk = 49,
  kTrailer = 50,
  kTransferEncoding = 51,
  kUpgrade = 52,
  kVary = 53,
  kVia = 54,
  kWarning = 55,
  kWWWAuthenticate = 56,
  kXContentTypeOptions = 57,
  kXDNSPrefetchControl = 58,
  kXFrameOptions = 59,
  kXXSSProtection = 60,
  kMaxValue = kXXSSProtection
};

struct IgnoredAction {
  IgnoredAction(extensions::ExtensionId extension_id,
                extensions::api::web_request::IgnoredActionType action_type);
  IgnoredAction(IgnoredAction&& rhs);

  extensions::ExtensionId extension_id;
  extensions::api::web_request::IgnoredActionType action_type;

 private:
  DISALLOW_COPY_AND_ASSIGN(IgnoredAction);
};

using IgnoredActions = std::vector<IgnoredAction>;

// Internal representation of the extraInfoSpec parameter on webRequest
// events, used to specify extra information to be included with network
// events.
struct ExtraInfoSpec {
  enum Flags {
    REQUEST_HEADERS = 1 << 0,
    RESPONSE_HEADERS = 1 << 1,
    BLOCKING = 1 << 2,
    ASYNC_BLOCKING = 1 << 3,
    REQUEST_BODY = 1 << 4,
    EXTRA_HEADERS = 1 << 5,
  };

  static bool InitFromValue(content::BrowserContext* browser_context,
                            const base::ListValue& value,
                            int* extra_info_spec);
};

// Data container for RequestCookies as defined in the declarative WebRequest
// API definition.
struct RequestCookie {
  RequestCookie();
  RequestCookie(RequestCookie&& other);
  RequestCookie& operator=(RequestCookie&& other);
  ~RequestCookie();

  bool operator==(const RequestCookie& other) const;

  RequestCookie Clone() const;

  base::Optional<std::string> name;
  base::Optional<std::string> value;

  DISALLOW_COPY_AND_ASSIGN(RequestCookie);
};

// Data container for ResponseCookies as defined in the declarative WebRequest
// API definition.
struct ResponseCookie {
  ResponseCookie();
  ResponseCookie(ResponseCookie&& other);
  ResponseCookie& operator=(ResponseCookie&& other);
  ~ResponseCookie();

  bool operator==(const ResponseCookie& other) const;

  ResponseCookie Clone() const;

  base::Optional<std::string> name;
  base::Optional<std::string> value;
  base::Optional<std::string> expires;
  base::Optional<int> max_age;
  base::Optional<std::string> domain;
  base::Optional<std::string> path;
  base::Optional<bool> secure;
  base::Optional<bool> http_only;

  DISALLOW_COPY_AND_ASSIGN(ResponseCookie);
};

// Data container for FilterResponseCookies as defined in the declarative
// WebRequest API definition.
struct FilterResponseCookie : ResponseCookie {
  FilterResponseCookie();
  FilterResponseCookie(FilterResponseCookie&& other);
  FilterResponseCookie& operator=(FilterResponseCookie&& other);
  ~FilterResponseCookie();

  FilterResponseCookie Clone() const;

  bool operator==(const FilterResponseCookie& other) const;

  base::Optional<int> age_lower_bound;
  base::Optional<int> age_upper_bound;
  base::Optional<bool> session_cookie;

  DISALLOW_COPY_AND_ASSIGN(FilterResponseCookie);
};

enum CookieModificationType {
  ADD,
  EDIT,
  REMOVE,
};

struct RequestCookieModification {
  RequestCookieModification();
  RequestCookieModification(RequestCookieModification&& other);
  RequestCookieModification& operator=(RequestCookieModification&& other);
  ~RequestCookieModification();

  bool operator==(const RequestCookieModification& other) const;

  RequestCookieModification Clone() const;

  CookieModificationType type;
  // Used for EDIT and REMOVE, nullopt otherwise.
  base::Optional<RequestCookie> filter;
  // Used for ADD and EDIT, nullopt otherwise.
  base::Optional<RequestCookie> modification;

  DISALLOW_COPY_AND_ASSIGN(RequestCookieModification);
};

struct ResponseCookieModification {
  ResponseCookieModification();
  ResponseCookieModification(ResponseCookieModification&& other);
  ResponseCookieModification& operator=(ResponseCookieModification&& other);
  ~ResponseCookieModification();

  bool operator==(const ResponseCookieModification& other) const;

  ResponseCookieModification Clone() const;

  CookieModificationType type;
  // Used for EDIT and REMOVE, nullopt otherwise.
  base::Optional<FilterResponseCookie> filter;
  // Used for ADD and EDIT, nullopt otherwise.
  base::Optional<ResponseCookie> modification;

  DISALLOW_COPY_AND_ASSIGN(ResponseCookieModification);
};

using RequestCookieModifications = std::vector<RequestCookieModification>;
using ResponseCookieModifications = std::vector<ResponseCookieModification>;

// Contains the modification an extension wants to perform on an event.
struct EventResponseDelta {
  EventResponseDelta(const std::string& extension_id,
                     const base::Time& extension_install_time);
  EventResponseDelta(EventResponseDelta&& other);
  EventResponseDelta& operator=(EventResponseDelta&& other);
  ~EventResponseDelta();

  // ID of the extension that sent this response.
  std::string extension_id;

  // The time that the extension was installed. Used for deciding order of
  // precedence in case multiple extensions respond with conflicting
  // decisions.
  base::Time extension_install_time;

  // Response values. These are mutually exclusive.
  bool cancel;
  GURL new_url;

  // Newly introduced or overridden request headers.
  net::HttpRequestHeaders modified_request_headers;

  // Keys of request headers to be deleted.
  std::vector<std::string> deleted_request_headers;

  // Headers that were added to the response. A modification of a header
  // corresponds to a deletion and subsequent addition of the new header.
  ResponseHeaders added_response_headers;

  // Headers that were deleted from the response.
  ResponseHeaders deleted_response_headers;

  // Authentication Credentials to use.
  base::Optional<net::AuthCredentials> auth_credentials;

  // Modifications to cookies in request headers.
  RequestCookieModifications request_cookie_modifications;

  // Modifications to cookies in response headers.
  ResponseCookieModifications response_cookie_modifications;

  // Messages that shall be sent to the background/event/... pages of the
  // extension.
  std::set<std::string> messages_to_extension;

  DISALLOW_COPY_AND_ASSIGN(EventResponseDelta);
};

using EventResponseDeltas = std::list<EventResponseDelta>;

// Comparison operator that returns true if the extension that caused
// |a| was installed after the extension that caused |b|.
bool InDecreasingExtensionInstallationTimeOrder(const EventResponseDelta& a,
                                                const EventResponseDelta& b);

// Converts a string to a list of integers, each in 0..255.
std::unique_ptr<base::ListValue> StringToCharList(const std::string& s);

// Converts a list of integer values between 0 and 255 into a string |*out|.
// Returns true if the conversion was successful.
bool CharListToString(const base::ListValue* list, std::string* out);

// The following functions calculate and return the modifications to requests
// commanded by extension handlers. All functions take the id of the extension
// that commanded a modification, the installation time of this extension (used
// for defining a precedence in conflicting modifications) and whether the
// extension requested to |cancel| the request. Other parameters depend on a
// the signal handler.

EventResponseDelta CalculateOnBeforeRequestDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const GURL& new_url);
EventResponseDelta CalculateOnBeforeSendHeadersDelta(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    net::HttpRequestHeaders* old_headers,
    net::HttpRequestHeaders* new_headers,
    int extra_info_spec);
EventResponseDelta CalculateOnHeadersReceivedDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const GURL& old_url,
    const GURL& new_url,
    const net::HttpResponseHeaders* old_response_headers,
    ResponseHeaders* new_response_headers,
    int extra_info_spec);
EventResponseDelta CalculateOnAuthRequiredDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    base::Optional<net::AuthCredentials> auth_credentials);

// These functions merge the responses (the |deltas|) of request handlers.
// The |deltas| need to be sorted in decreasing order of precedence of
// extensions. In case extensions had |deltas| that could not be honored, their
// IDs are reported in |conflicting_extensions|.

// Stores in |canceled| whether any extension wanted to cancel the request.
void MergeCancelOfResponses(const EventResponseDeltas& deltas, bool* canceled);
// Stores in |*new_url| the redirect request of the extension with highest
// precedence. Extensions that did not command to redirect the request are
// ignored in this logic.
void MergeRedirectUrlOfResponses(const GURL& url,
                                 const EventResponseDeltas& deltas,
                                 GURL* new_url,
                                 IgnoredActions* ignored_actions);
// Stores in |*new_url| the redirect request of the extension with highest
// precedence. Extensions that did not command to redirect the request are
// ignored in this logic.
void MergeOnBeforeRequestResponses(const GURL& url,
                                   const EventResponseDeltas& deltas,
                                   GURL* new_url,
                                   IgnoredActions* ignored_actions);
// Modifies the "Cookie" header in |request_headers| according to
// |deltas.request_cookie_modifications|. Conflicts are currently ignored
// silently.
void MergeCookiesInOnBeforeSendHeadersResponses(
    const GURL& gurl,
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers);
// Modifies the headers in |request_headers| according to |deltas|. Conflicts
// are tried to be resolved.
// Stores in |request_headers_modified| whether the request headers were
// modified.
void MergeOnBeforeSendHeadersResponses(
    const extensions::WebRequestInfo& request,
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers,
    IgnoredActions* ignored_actions,
    std::set<std::string>* removed_headers,
    std::set<std::string>* set_headers,
    bool* request_headers_modified);
// Modifies the "Set-Cookie" headers in |override_response_headers| according to
// |deltas.response_cookie_modifications|. If |override_response_headers| is
// NULL, a copy of |original_response_headers| is created. Conflicts are
// currently ignored silently.
void MergeCookiesInOnHeadersReceivedResponses(
    const GURL& url,
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers);
// Stores a copy of |original_response_header| into |override_response_headers|
// that is modified according to |deltas|. If |deltas| does not instruct to
// modify the response headers, |override_response_headers| remains empty.
// Extension-initiated redirects are written to |override_response_headers|
// (to request redirection) and |*preserve_fragment_on_redirect_url| (to make
// sure that the URL provided by the extension isn't modified by having its
// fragment overwritten by that of the original URL). Stores in
// |response_headers_modified| whether the response headers were modified.
void MergeOnHeadersReceivedResponses(
    const extensions::WebRequestInfo& request,
    const EventResponseDeltas& deltas,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* preserve_fragment_on_redirect_url,
    IgnoredActions* ignored_actions,
    bool* response_headers_modified);
// Merge the responses of blocked onAuthRequired handlers. The first
// registered listener that supplies authentication credentials in a response,
// if any, will have its authentication credentials used. |request| must be
// non-NULL, and contain |deltas| that are sorted in decreasing order of
// precedence.
// Returns whether authentication credentials are set.
bool MergeOnAuthRequiredResponses(const EventResponseDeltas& deltas,
                                  net::AuthCredentials* auth_credentials,
                                  IgnoredActions* ignored_actions);

// Triggers clearing each renderer's in-memory cache the next time it navigates.
void ClearCacheOnNavigation();

// Converts the |name|, |value| pair of a http header to a HttpHeaders
// dictionary.
std::unique_ptr<base::DictionaryValue> CreateHeaderDictionary(
    const std::string& name,
    const std::string& value);

// Returns whether a request header should be hidden from listeners.
bool ShouldHideRequestHeader(content::BrowserContext* browser_context,
                             int extra_info_spec,
                             const std::string& name);

// Returns whether a response header should be hidden from listeners.
bool ShouldHideResponseHeader(int extra_info_spec, const std::string& name);

}  // namespace extension_web_request_api_helpers

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_HELPERS_H_
