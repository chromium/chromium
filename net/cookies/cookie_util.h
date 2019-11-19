// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_UTIL_H_
#define NET_COOKIES_COOKIE_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "url/origin.h"

class GURL;

namespace net {
namespace cookie_util {

// Constants for use in VLOG
const int kVlogPerCookieMonster = 1;
const int kVlogSetCookies = 7;
const int kVlogGarbageCollection = 5;

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
                                          std::string* result);

// Returns true if a domain string represents a host-only cookie,
// i.e. it doesn't begin with a leading '.' character.
NET_EXPORT bool DomainIsHostOnly(const std::string& domain_string);

// Parses the string with the cookie expiration time (very forgivingly).
// Returns the "null" time on failure.
//
// If the expiration date is below or above the platform-specific range
// supported by Time::FromUTCExplodeded(), then this will return Time(1) or
// Time::Max(), respectively.
NET_EXPORT base::Time ParseCookieExpirationTime(const std::string& time_string);

// Convenience for converting a cookie origin (domain and https pair) to a URL.
NET_EXPORT GURL CookieOriginToURL(const std::string& domain, bool is_https);

// Returns true if the cookie |domain| matches the given |host| as described
// in section 5.1.3 of RFC 6265.
NET_EXPORT bool IsDomainMatch(const std::string& domain,
                              const std::string& host);

// A ParsedRequestCookie consists of the key and value of the cookie.
using ParsedRequestCookie = std::pair<std::string, std::string>;
using ParsedRequestCookies = std::vector<ParsedRequestCookie>;

// Assumes that |header_value| is the cookie header value of a HTTP Request
// following the cookie-string schema of RFC 6265, section 4.2.1, and returns
// cookie name/value pairs. If cookie values are presented in double quotes,
// these will appear in |parsed_cookies| as well. Assumes that the cookie
// header is written by Chromium and therefore well-formed.
NET_EXPORT void ParseRequestCookieLine(const std::string& header_value,
                                       ParsedRequestCookies* parsed_cookies);

// Writes all cookies of |parsed_cookies| into a HTTP Request header value
// that belongs to the "Cookie" header. The entries of |parsed_cookies| must
// already be appropriately escaped.
NET_EXPORT std::string SerializeRequestCookieLine(
    const ParsedRequestCookies& parsed_cookies);

// Determines which of the cookies for |url| can be accessed, with respect to
// the SameSite attribute. This applies to looking up existing cookies; for
// setting new ones, see ComputeSameSiteContextForResponse and
// ComputeSameSiteContextForScriptSet.
//
// |site_for_cookies| is the currently navigated to site that should be
// considered "first-party" for cookies.
//
// |initiator| is the origin ultimately responsible for getting the request
// issued; it may be different from |site_for_cookies| in that it may be some
// other website that caused the navigation to |site_for_cookies| to occur.
//
// base::nullopt for |initiator| denotes that the navigation was initiated by
// the user directly interacting with the browser UI, e.g. entering a URL
// or selecting a bookmark.
//
// If |attach_same_site_cookies| is specified, all SameSite cookies will be
// attached, i.e. this will return SAME_SITE_STRICT. This flag is set to true
// when the |site_for_cookies| is a chrome:// URL embedding a secure origin,
// among other scenarios.
// This is *not* set when the *initiator* is chrome-extension://,
// which is intentional, since it would be bad to let an extension arbitrarily
// redirect anywhere and bypass SameSite=Strict rules.
//
// See also documentation for corresponding methods on net::URLRequest.
//
// |http_method| is used to enforce the requirement that, in a context that's
// lax same-site but not strict same-site, SameSite=lax cookies be only sent
// when the method is "safe" in the RFC7231 section 4.2.1 sense.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForRequest(const std::string& http_method,
                                 const GURL& url,
                                 const GURL& site_for_cookies,
                                 const base::Optional<url::Origin>& initiator,
                                 bool attach_same_site_cookies);

// As above, but applying for scripts. |initiator| here should be the initiator
// used when fetching the document.
// If |attach_same_site_cookies| is true, this returns SAME_SITE_STRICT.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForScriptGet(const GURL& url,
                                   const GURL& site_for_cookies,
                                   const base::Optional<url::Origin>& initiator,
                                   bool attach_same_site_cookies);

// Determines which of the cookies for |url| can be set from a network response,
// with respect to the SameSite attribute. This will only return CROSS_SITE or
// SAME_SITE_LAX (cookie sets of SameSite=strict cookies are permitted in same
// contexts that sets of SameSite=lax cookies are).
// If |attach_same_site_cookies| is true, this returns SAME_SITE_LAX.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForResponse(const GURL& url,
                                  const GURL& site_for_cookies,
                                  const base::Optional<url::Origin>& initiator,
                                  bool attach_same_site_cookies);

// Determines which of the cookies for |url| can be set from a script context,
// with respect to the SameSite attribute. This will only return CROSS_SITE or
// SAME_SITE_LAX (cookie sets of SameSite=strict cookies are permitted in same
// contexts that sets of SameSite=lax cookies are).
// If |attach_same_site_cookies| is true, this returns SAME_SITE_LAX.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForScriptSet(const GURL& url,
                                   const GURL& site_for_cookies,
                                   bool attach_same_site_cookies);

// Determines which of the cookies for |url| can be accessed when fetching a
// subresources. This is either CROSS_SITE or SAME_SITE_STRICT,
// since the initiator for a subresource is the frame loading it.
NET_EXPORT CookieOptions::SameSiteCookieContext
// If |attach_same_site_cookies| is true, this returns SAME_SITE_STRICT.
ComputeSameSiteContextForSubresource(const GURL& url,
                                     const GURL& site_for_cookies,
                                     bool attach_same_site_cookies);

// Returns whether the respective SameSite feature is enabled.
NET_EXPORT bool IsSameSiteByDefaultCookiesEnabled();
NET_EXPORT bool IsCookiesWithoutSameSiteMustBeSecureEnabled();

// Takes a callback accepting a CookieInclusionStatus and returns a callback
// that accepts a bool, setting the bool to true if the CookieInclusionStatus
// was set to "include", else sending false.
//
// Can be used with SetCanonicalCookie when you don't need to know why a cookie
// was blocked, only whether it was blocked.
NET_EXPORT base::OnceCallback<void(CanonicalCookie::CookieInclusionStatus)>
AdaptCookieInclusionStatusToBool(base::OnceCallback<void(bool)> callback);

// Turn a CookieStatusList into a CookieList by stripping out the statuses
// (for callers who don't care about the statuses).
NET_EXPORT CookieList StripStatuses(const CookieStatusList& cookie_status_list);

}  // namespace cookie_util
}  // namespace net

#endif  // NET_COOKIES_COOKIE_UTIL_H_
