// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_UTIL_H_
#define NET_COOKIES_COOKIE_UTIL_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/site_for_cookies.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class GURL;

namespace net {

class IsolationInfo;
class SchemefulSite;
class CookieAccessDelegate;
class CookieInclusionStatus;

namespace cookie_util {

// Constants for use in VLOG
const int kVlogPerCookieMonster = 1;
const int kVlogSetCookies = 7;
const int kVlogGarbageCollection = 5;

// This enum must match the numbering for StorageAccessResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// at the end.
enum class StorageAccessResult {
  ACCESS_BLOCKED = 0,
  ACCESS_ALLOWED = 1,
  ACCESS_ALLOWED_STORAGE_ACCESS_GRANT = 2,
  ACCESS_ALLOWED_FORCED = 3,
  ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT = 4,
  kMaxValue = ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT,
};
// Helper to fire telemetry indicating if a given request for storage was
// allowed or not by the provided |result|.
NET_EXPORT void FireStorageAccessHistogram(StorageAccessResult result);

// This enum must match the numbering for StorageAccessInputState in
// histograms/enums.xml. Do not reorder or remove items, only add new items at
// the end.
enum class StorageAccessInputState {
  // The frame-level opt-in was provided, and a permission grant exists.
  kOptInWithGrant = 0,
  // The frame-level opt-in was provided, but no permission grant exists.
  kOptInWithoutGrant = 1,
  // No frame-level opt-in was provided, but a permission grant exists.
  kGrantWithoutOptIn = 2,
  // No frame-level opt-in was provided, and no permission grant exists.
  kNoOptInNoGrant = 3,
  kMaxValue = kNoOptInNoGrant,
};
// Helper to record a histogram sample for relevant Storage Access API state
// when cookie settings queries consult the Storage Access API grants.
NET_EXPORT void FireStorageAccessInputHistogram(bool has_opt_in,
                                                bool has_grant);

// Returns the effective TLD+1 for a given host. This only makes sense for http
// and https schemes. For other schemes, the host will be returned unchanged
// (minus any leading period).
NET_EXPORT std::string GetEffectiveDomain(const std::string& scheme,
                                          const std::string& host);

// Determine the actual cookie domain based on the domain string passed
// (if any) and the URL from which the cookie came.
// On success returns true, and sets cookie_domain to either a
//   -host cookie domain (ex: "google.com")
//   -domain cookie domain (ex: ".google.com")
// On success, DomainIsHostOnly(url.host()) is DCHECKed. The URL's host must not
// begin with a '.' character.
NET_EXPORT bool GetCookieDomainWithString(const GURL& url,
                                          const std::string& domain_string,
                                          CookieInclusionStatus& status,
                                          std::string* result);

// Returns true if a domain string represents a host-only cookie,
// i.e. it doesn't begin with a leading '.' character.
NET_EXPORT bool DomainIsHostOnly(const std::string& domain_string);

// If |cookie_domain| is nonempty and starts with a "." character, this returns
// the substring of |cookie_domain| without the leading dot. (Note only one
// leading dot is stripped, if there are multiple.) Otherwise it returns
// |cookie_domain|. This is useful for converting from CanonicalCookie's
// representation of a cookie domain to the RFC's notion of a cookie's domain.
NET_EXPORT std::string CookieDomainAsHost(const std::string& cookie_domain);

// Parses the string with the cookie expiration time (very forgivingly).
// Returns the "null" time on failure.
//
// If the expiration date is below or above the platform-specific range
// supported by Time::FromUTCExplodeded(), then this will return Time(1) or
// Time::Max(), respectively.
NET_EXPORT base::Time ParseCookieExpirationTime(const std::string& time_string);

// Get a cookie's URL from it's domain, path, and source scheme.
// The first field can be the combined domain-and-host-only-flag (e.g. the
// string returned by CanonicalCookie::Domain()) as opposed to the domain
// attribute per RFC6265bis. The GURL is constructed after stripping off any
// leading dot.
// Note: the GURL returned by this method is not guaranteed to be valid.
NET_EXPORT GURL CookieDomainAndPathToURL(const std::string& domain,
                                         const std::string& path,
                                         const std::string& source_scheme);
NET_EXPORT GURL CookieDomainAndPathToURL(const std::string& domain,
                                         const std::string& path,
                                         bool is_https);
NET_EXPORT GURL CookieDomainAndPathToURL(const std::string& domain,
                                         const std::string& path,
                                         CookieSourceScheme source_scheme);

// Convenience for converting a cookie origin (domain and https pair) to a URL.
NET_EXPORT GURL CookieOriginToURL(const std::string& domain, bool is_https);

// Returns a URL that could have been the cookie's source.
// Not guaranteed to actually be the URL that set the cookie. Not guaranteed to
// be a valid GURL. Intended as a shim for SetCanonicalCookieAsync calls, where
// a source URL is required but only a source scheme may be available.
NET_EXPORT GURL SimulatedCookieSource(const CanonicalCookie& cookie,
                                      const std::string& source_scheme);

// Provisional evaluation of acceptability of setting secure cookies on
// `source_url` based only on the `source_url`'s scheme and whether it
// is a localhost URL.  If this returns kNonCryptographic, it may be upgraded to
// kTrustworthy by a CookieAccessDelegate when the cookie operation is being
// performed, as the delegate may have access to user settings like manually
// configured test domains which declare additional things trustworthy.
NET_EXPORT CookieAccessScheme ProvisionalAccessScheme(const GURL& source_url);

// |domain| is the output of cookie.Domain() for some cookie. This returns true
// if a |domain| indicates that the cookie can be accessed by |host|.
// See comment on CanonicalCookie::IsDomainMatch().
NET_EXPORT bool IsDomainMatch(const std::string& domain,
                              const std::string& host);

// Returns true if the given |url_path| path-matches |cookie_path|
// as described in section 5.1.4 in RFC 6265. This returns true if |cookie_path|
// and |url_path| are identical, or if |url_path| is a subdirectory of
// |cookie_path|.
NET_EXPORT bool IsOnPath(const std::string& cookie_path,
                         const std::string& url_path);

// A ParsedRequestCookie consists of the key and value of the cookie.
using ParsedRequestCookie = std::pair<std::string, std::string>;
using ParsedRequestCookies = std::vector<ParsedRequestCookie>;

// Assumes that |header_value| is the cookie header value of a HTTP Request
// following the cookie-string schema of RFC 6265, section 4.2.1, and returns
// cookie name/value pairs. If cookie values are presented in double quotes,
// these will appear in |parsed_cookies| as well. The cookie header can be
// written by non-Chromium consumers (such as extensions), so the header may not
// be well-formed.
NET_EXPORT void ParseRequestCookieLine(const std::string& header_value,
                                       ParsedRequestCookies* parsed_cookies);

// Writes all cookies of |parsed_cookies| into a HTTP Request header value
// that belongs to the "Cookie" header. The entries of |parsed_cookies| must
// already be appropriately escaped.
NET_EXPORT std::string SerializeRequestCookieLine(
    const ParsedRequestCookies& parsed_cookies);

// Determines which of the cookies for the request URL can be accessed, with
// respect to the SameSite attribute. This applies to looking up existing
// cookies for HTTP requests. For looking up cookies for non-HTTP APIs (i.e.,
// JavaScript), see ComputeSameSiteContextForScriptGet. For setting new cookies,
// see ComputeSameSiteContextForResponse and ComputeSameSiteContextForScriptSet.
//
// `url_chain` is a non-empty vector of URLs, the last of which is the current
// request URL. It represents the redirect chain of the current request. The
// redirect chain is used to calculate whether there has been a cross-site
// redirect. In order for a context to be deemed strictly same-site, there must
// not have been any cross-site redirects.
//
// `site_for_cookies` is the currently navigated to site that should be
// considered "first-party" for cookies.
//
// `initiator` is the origin ultimately responsible for getting the request
// issued. It may be different from `site_for_cookies`.
//
// absl::nullopt for `initiator` denotes that the navigation was initiated by
// the user directly interacting with the browser UI, e.g. entering a URL
// or selecting a bookmark.
//
// `is_main_frame_navigation` is whether the request is for a navigation that
// targets the main frame or top-level browsing context. These requests may
// sometimes send SameSite=Lax cookies but not SameSite=Strict cookies.
//
// If `force_ignore_site_for_cookies` is specified, all SameSite cookies will be
// attached, i.e. this will return SAME_SITE_STRICT. This flag is set to true
// when the `site_for_cookies` is a chrome:// URL embedding a secure origin,
// among other scenarios.
// This is *not* set when the *initiator* is chrome-extension://,
// which is intentional, since it would be bad to let an extension arbitrarily
// redirect anywhere and bypass SameSite=Strict rules.
//
// See also documentation for corresponding methods on net::URLRequest.
//
// `http_method` is used to enforce the requirement that, in a context that's
// lax same-site but not strict same-site, SameSite=lax cookies be only sent
// when the method is "safe" in the RFC7231 section 4.2.1 sense.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForRequest(const std::string& http_method,
                                 const std::vector<GURL>& url_chain,
                                 const SiteForCookies& site_for_cookies,
                                 const absl::optional<url::Origin>& initiator,
                                 bool is_main_frame_navigation,
                                 bool force_ignore_site_for_cookies);

// As above, but applying for scripts. `initiator` here should be the initiator
// used when fetching the document.
// If `force_ignore_site_for_cookies` is true, this returns SAME_SITE_STRICT.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForScriptGet(const GURL& url,
                                   const SiteForCookies& site_for_cookies,
                                   const absl::optional<url::Origin>& initiator,
                                   bool force_ignore_site_for_cookies);

// Determines which of the cookies for the request URL can be set from a network
// response, with respect to the SameSite attribute. This will only return
// CROSS_SITE or SAME_SITE_LAX (cookie sets of SameSite=strict cookies are
// permitted in same contexts that sets of SameSite=lax cookies are).
// `url_chain` is a non-empty vector of URLs, the last of which is the current
// request URL. It represents the redirect chain of the current request. The
// redirect chain is used to calculate whether there has been a cross-site
// redirect.
// `is_main_frame_navigation` is whether the request was for a navigation that
// targets the main frame or top-level browsing context. Both SameSite=Lax and
// SameSite=Strict cookies may be set by any main frame navigation.
// If `force_ignore_site_for_cookies` is true, this returns SAME_SITE_LAX.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForResponse(const std::vector<GURL>& url_chain,
                                  const SiteForCookies& site_for_cookies,
                                  const absl::optional<url::Origin>& initiator,
                                  bool is_main_frame_navigation,
                                  bool force_ignore_site_for_cookies);

// Determines which of the cookies for `url` can be set from a script context,
// with respect to the SameSite attribute. This will only return CROSS_SITE or
// SAME_SITE_LAX (cookie sets of SameSite=strict cookies are permitted in same
// contexts that sets of SameSite=lax cookies are).
// If `force_ignore_site_for_cookies` is true, this returns SAME_SITE_LAX.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForScriptSet(const GURL& url,
                                   const SiteForCookies& site_for_cookies,
                                   bool force_ignore_site_for_cookies);

// Determines which of the cookies for |url| can be accessed when fetching a
// subresources. This is either CROSS_SITE or SAME_SITE_STRICT,
// since the initiator for a subresource is the frame loading it.
NET_EXPORT CookieOptions::SameSiteCookieContext
// If |force_ignore_site_for_cookies| is true, this returns SAME_SITE_STRICT.
ComputeSameSiteContextForSubresource(const GURL& url,
                                     const SiteForCookies& site_for_cookies,
                                     bool force_ignore_site_for_cookies);

NET_EXPORT bool IsPortBoundCookiesEnabled();

NET_EXPORT bool IsSchemeBoundCookiesEnabled();

// Returns whether the respective feature is enabled.
NET_EXPORT bool IsSchemefulSameSiteEnabled();

// Computes the First-Party Sets metadata, determining which of the cookies for
// `request_site` can be accessed. `isolation_info` must be fully populated.  If
// `force_ignore_top_frame_party` is true, the top frame from `isolation_info`
// will be assumed to be same-party with `request_site`, regardless of what it
// is.
//
// The result may be returned synchronously, or `callback` may be invoked
// asynchronously with the result. The callback will be invoked iff the return
// value is nullopt; i.e. a result will be provided via return value or
// callback, but not both, and not neither.
[[nodiscard]] NET_EXPORT absl::optional<FirstPartySetMetadata>
ComputeFirstPartySetMetadataMaybeAsync(
    const SchemefulSite& request_site,
    const IsolationInfo& isolation_info,
    const CookieAccessDelegate* cookie_access_delegate,
    bool force_ignore_top_frame_party,
    base::OnceCallback<void(FirstPartySetMetadata)> callback);

// Converts a string representing the http request method to its enum
// representation.
NET_EXPORT CookieOptions::SameSiteCookieContext::ContextMetadata::HttpMethod
HttpMethodStringToEnum(const std::string& in);

// Get the SameParty inclusion status. If the cookie is not SameParty, returns
// kNoSamePartyEnforcement; if the cookie is SameParty but does not have a
// valid context, returns kEnforceSamePartyExclude.
NET_EXPORT CookieSamePartyStatus
GetSamePartyStatus(const CanonicalCookie& cookie,
                   const CookieOptions& options,
                   bool same_party_attribute_enabled);

// Takes a CookieAccessResult and returns a bool, returning true if the
// CookieInclusionStatus in CookieAccessResult was set to "include", else
// returning false.
//
// Can be used with SetCanonicalCookie when you don't need to know why a cookie
// was blocked, only whether it was blocked.
NET_EXPORT bool IsCookieAccessResultInclude(
    CookieAccessResult cookie_access_result);

// Turn a CookieAccessResultList into a CookieList by stripping out access
// results (for callers who only care about cookies).
NET_EXPORT CookieList
StripAccessResults(const CookieAccessResultList& cookie_access_result_list);

// Records port related metrics from Omnibox navigations.
NET_EXPORT void RecordCookiePortOmniboxHistograms(const GURL& url);

// Checks invariants that should be upheld w.r.t. the included and excluded
// cookies. Namely: the included cookies should be elements of
// `included_cookies`; excluded cookies should be elements of
// `excluded_cookies`; and included cookies should be in the correct sorted
// order.
NET_EXPORT void DCheckIncludedAndExcludedCookieLists(
    const CookieAccessResultList& included_cookies,
    const CookieAccessResultList& excluded_cookies);

// Returns the default third-party cookie blocking setting, which is false
// unless you enable ForceThirdPartyCookieBlocking with the command line switch
// --test-third-party-cookie-phaseout.
NET_EXPORT bool IsForceThirdPartyCookieBlockingEnabled();

}  // namespace cookie_util

}  // namespace net

#endif  // NET_COOKIES_COOKIE_UTIL_H_
