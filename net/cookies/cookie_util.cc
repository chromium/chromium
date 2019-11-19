// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_util.h"

#include <cstdio>
#include <cstdlib>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

namespace net {
namespace cookie_util {

namespace {

base::Time MinNonNullTime() {
  return base::Time::FromInternalValue(1);
}

// Tries to assemble a base::Time given a base::Time::Exploded representing a
// UTC calendar date.
//
// If the date falls outside of the range supported internally by
// FromUTCExploded() on the current platform, then the result is:
//
// * Time(1) if it's below the range FromUTCExploded() supports.
// * Time::Max() if it's above the range FromUTCExploded() supports.
bool SaturatedTimeFromUTCExploded(const base::Time::Exploded& exploded,
                                  base::Time* out) {
  // Try to calculate the base::Time in the normal fashion.
  if (base::Time::FromUTCExploded(exploded, out)) {
    // Don't return Time(0) on success.
    if (out->is_null())
      *out = MinNonNullTime();
    return true;
  }

  // base::Time::FromUTCExploded() has platform-specific limits:
  //
  // * Windows: Years 1601 - 30827
  // * 32-bit POSIX: Years 1970 - 2038
  //
  // Work around this by returning min/max valid times for times outside those
  // ranges when imploding the time is doomed to fail.
  //
  // Note that the following implementation is NOT perfect. It will accept
  // some invalid calendar dates in the out-of-range case.
  if (!exploded.HasValidValues())
    return false;

  if (exploded.year > base::Time::kExplodedMaxYear) {
    *out = base::Time::Max();
    return true;
  }
  if (exploded.year < base::Time::kExplodedMinYear) {
    *out = MinNonNullTime();
    return true;
  }

  return false;
}

bool MatchesSiteForCookies(const GURL& url, const GURL& site_for_cookies) {
  return registry_controlled_domains::SameDomainOrHost(
      url, site_for_cookies,
      registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

CookieOptions::SameSiteCookieContext ComputeSchemeChange(
    CookieOptions::SameSiteCookieContext same_site_type,
    const GURL& url,
    const GURL& site_for_cookies) {
  if (site_for_cookies.is_empty())
    return same_site_type;

  DCHECK(same_site_type ==
             CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT ||
         same_site_type == CookieOptions::SameSiteCookieContext::SAME_SITE_LAX);

  bool url_secure = url.SchemeIsCryptographic();
  bool site_for_cookies_secure = site_for_cookies.SchemeIsCryptographic();

  // Check for different schemes and add flag if so.
  if (url_secure && !site_for_cookies_secure) {
    same_site_type = CookieOptions::ApplyCrossSchemeBitmask(
        same_site_type, CookieOptions::kToSecureMask);
  } else if (!url_secure && site_for_cookies_secure) {
    same_site_type = CookieOptions::ApplyCrossSchemeBitmask(
        same_site_type, CookieOptions::kToInsecureMask);
  }

  return same_site_type;
}

CookieOptions::SameSiteCookieContext ComputeSameSiteContext(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& initiator) {
  if (MatchesSiteForCookies(url, site_for_cookies)) {
    CookieOptions::SameSiteCookieContext same_site_type;
    if (!initiator ||
        registry_controlled_domains::SameDomainOrHost(
            url, initiator.value(),
            registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      same_site_type = CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT;
    } else {
      same_site_type = CookieOptions::SameSiteCookieContext::SAME_SITE_LAX;
    }

    same_site_type = ComputeSchemeChange(same_site_type, url, site_for_cookies);

    return same_site_type;
  }
  return CookieOptions::SameSiteCookieContext::CROSS_SITE;
}

}  // namespace

bool DomainIsHostOnly(const std::string& domain_string) {
  return (domain_string.empty() || domain_string[0] != '.');
}

std::string GetEffectiveDomain(const std::string& scheme,
                               const std::string& host) {
  if (scheme == "http" || scheme == "https" || scheme == "ws" ||
      scheme == "wss") {
    return registry_controlled_domains::GetDomainAndRegistry(
        host,
        registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  }

  if (!DomainIsHostOnly(host))
    return host.substr(1);
  return host;
}

bool GetCookieDomainWithString(const GURL& url,
                               const std::string& domain_string,
                               std::string* result) {
  const std::string url_host(url.host());

  url::CanonHostInfo ignored;
  std::string cookie_domain(CanonicalizeHost(domain_string, &ignored));

  // If no domain was specified in the domain string, default to a host cookie.
  // We match IE/Firefox in allowing a domain=IPADDR if it matches the url
  // ip address hostname exactly.  It should be treated as a host cookie.
  if (domain_string.empty() ||
      (url.HostIsIPAddress() && url_host == cookie_domain)) {
    *result = url_host;
    DCHECK(DomainIsHostOnly(*result));
    return true;
  }

  // Disallow domain names with %-escaped characters.
  for (char c : domain_string) {
    if (c == '%')
      return false;
  }

  // Get the normalized domain specified in cookie line.
  if (cookie_domain.empty())
    return false;
  if (cookie_domain[0] != '.')
    cookie_domain = "." + cookie_domain;

  // Ensure |url| and |cookie_domain| have the same domain+registry.
  const std::string url_scheme(url.scheme());
  const std::string url_domain_and_registry(
      GetEffectiveDomain(url_scheme, url_host));
  if (url_domain_and_registry.empty()) {
    // We match IE/Firefox by treating an exact match between the domain
    // attribute and the request host to be treated as a host cookie.
    if (url_host == domain_string) {
      *result = url_host;
      DCHECK(DomainIsHostOnly(*result));
      return true;
    }

    // Otherwise, IP addresses/intranet hosts/public suffixes can't set
    // domain cookies.
    return false;
  }
  const std::string cookie_domain_and_registry(
      GetEffectiveDomain(url_scheme, cookie_domain));
  if (url_domain_and_registry != cookie_domain_and_registry)
    return false;  // Can't set a cookie on a different domain + registry.

  // Ensure |url_host| is |cookie_domain| or one of its subdomains.  Given that
  // we know the domain+registry are the same from the above checks, this is
  // basically a simple string suffix check.
  const bool is_suffix = (url_host.length() < cookie_domain.length()) ?
      (cookie_domain != ("." + url_host)) :
      (url_host.compare(url_host.length() - cookie_domain.length(),
                        cookie_domain.length(), cookie_domain) != 0);
  if (is_suffix)
    return false;

  *result = cookie_domain;
  return true;
}

// Parse a cookie expiration time.  We try to be lenient, but we need to
// assume some order to distinguish the fields.  The basic rules:
//  - The month name must be present and prefix the first 3 letters of the
//    full month name (jan for January, jun for June).
//  - If the year is <= 2 digits, it must occur after the day of month.
//  - The time must be of the format hh:mm:ss.
// An average cookie expiration will look something like this:
//   Sat, 15-Apr-17 21:01:22 GMT
base::Time ParseCookieExpirationTime(const std::string& time_string) {
  static const char* const kMonths[] = {
    "jan", "feb", "mar", "apr", "may", "jun",
    "jul", "aug", "sep", "oct", "nov", "dec" };
  // We want to be pretty liberal, and support most non-ascii and non-digit
  // characters as a delimiter.  We can't treat : as a delimiter, because it
  // is the delimiter for hh:mm:ss, and we want to keep this field together.
  // We make sure to include - and +, since they could prefix numbers.
  // If the cookie attribute came in in quotes (ex expires="XXX"), the quotes
  // will be preserved, and we will get them here.  So we make sure to include
  // quote characters, and also \ for anything that was internally escaped.
  static const char kDelimiters[] = "\t !\"#$%&'()*+,-./;<=>?@[\\]^_`{|}~";

  base::Time::Exploded exploded = {0};

  base::StringTokenizer tokenizer(time_string, kDelimiters);

  bool found_day_of_month = false;
  bool found_month = false;
  bool found_time = false;
  bool found_year = false;

  while (tokenizer.GetNext()) {
    const std::string token = tokenizer.token();
    DCHECK(!token.empty());
    bool numerical = base::IsAsciiDigit(token[0]);

    // String field
    if (!numerical) {
      if (!found_month) {
        for (size_t i = 0; i < base::size(kMonths); ++i) {
          // Match prefix, so we could match January, etc
          if (base::StartsWith(token, base::StringPiece(kMonths[i], 3),
                               base::CompareCase::INSENSITIVE_ASCII)) {
            exploded.month = static_cast<int>(i) + 1;
            found_month = true;
            break;
          }
        }
      } else {
        // If we've gotten here, it means we've already found and parsed our
        // month, and we have another string, which we would expect to be the
        // the time zone name.  According to the RFC and my experiments with
        // how sites format their expirations, we don't have much of a reason
        // to support timezones.  We don't want to ever barf on user input,
        // but this DCHECK should pass for well-formed data.
        // DCHECK(token == "GMT");
      }
    // Numeric field w/ a colon
    } else if (token.find(':') != std::string::npos) {
      if (!found_time &&
#ifdef COMPILER_MSVC
          sscanf_s(
#else
          sscanf(
#endif
                 token.c_str(), "%2u:%2u:%2u", &exploded.hour,
                 &exploded.minute, &exploded.second) == 3) {
        found_time = true;
      } else {
        // We should only ever encounter one time-like thing.  If we're here,
        // it means we've found a second, which shouldn't happen.  We keep
        // the first.  This check should be ok for well-formed input:
        // NOTREACHED();
      }
    // Numeric field
    } else {
      // Overflow with atoi() is unspecified, so we enforce a max length.
      if (!found_day_of_month && token.length() <= 2) {
        exploded.day_of_month = atoi(token.c_str());
        found_day_of_month = true;
      } else if (!found_year && token.length() <= 5) {
        exploded.year = atoi(token.c_str());
        found_year = true;
      } else {
        // If we're here, it means we've either found an extra numeric field,
        // or a numeric field which was too long.  For well-formed input, the
        // following check would be reasonable:
        // NOTREACHED();
      }
    }
  }

  if (!found_day_of_month || !found_month || !found_time || !found_year) {
    // We didn't find all of the fields we need.  For well-formed input, the
    // following check would be reasonable:
    // NOTREACHED() << "Cookie parse expiration failed: " << time_string;
    return base::Time();
  }

  // Normalize the year to expand abbreviated years to the full year.
  if (exploded.year >= 69 && exploded.year <= 99)
    exploded.year += 1900;
  if (exploded.year >= 0 && exploded.year <= 68)
    exploded.year += 2000;

  // Note that clipping the date if it is outside of a platform-specific range
  // is permitted by: https://tools.ietf.org/html/rfc6265#section-5.2.1
  base::Time result;
  if (SaturatedTimeFromUTCExploded(exploded, &result))
    return result;

  // One of our values was out of expected range.  For well-formed input,
  // the following check would be reasonable:
  // NOTREACHED() << "Cookie exploded expiration failed: " << time_string;

  return base::Time();
}

GURL CookieOriginToURL(const std::string& domain, bool is_https) {
  if (domain.empty())
    return GURL();

  const std::string scheme = is_https ? "https" : "http";
  const std::string host = domain[0] == '.' ? domain.substr(1) : domain;
  return GURL(scheme + "://" + host);
}

bool IsDomainMatch(const std::string& domain, const std::string& host) {
  // Can domain match in two ways; as a domain cookie (where the cookie
  // domain begins with ".") or as a host cookie (where it doesn't).

  // Some consumers of the CookieMonster expect to set cookies on
  // URLs like http://.strange.url.  To retrieve cookies in this instance,
  // we allow matching as a host cookie even when the domain_ starts with
  // a period.
  if (host == domain)
    return true;

  // Domain cookie must have an initial ".".  To match, it must be
  // equal to url's host with initial period removed, or a suffix of
  // it.

  // Arguably this should only apply to "http" or "https" cookies, but
  // extension cookie tests currently use the funtionality, and if we
  // ever decide to implement that it should be done by preventing
  // such cookies from being set.
  if (domain.empty() || domain[0] != '.')
    return false;

  // The host with a "." prefixed.
  if (domain.compare(1, std::string::npos, host) == 0)
    return true;

  // A pure suffix of the host (ok since we know the domain already
  // starts with a ".")
  return (host.length() > domain.length() &&
          host.compare(host.length() - domain.length(), domain.length(),
                       domain) == 0);
}

void ParseRequestCookieLine(const std::string& header_value,
                            ParsedRequestCookies* parsed_cookies) {
  std::string::const_iterator i = header_value.begin();
  while (i != header_value.end()) {
    // Here we are at the beginning of a cookie.

    // Eat whitespace.
    while (i != header_value.end() && *i == ' ') ++i;
    if (i == header_value.end()) return;

    // Find cookie name.
    std::string::const_iterator cookie_name_beginning = i;
    while (i != header_value.end() && *i != '=') ++i;
    base::StringPiece cookie_name(cookie_name_beginning, i);

    // Find cookie value.
    base::StringPiece cookie_value;
    // Cookies may have no value, in this case '=' may or may not be there.
    if (i != header_value.end() && i + 1 != header_value.end()) {
      ++i;  // Skip '='.
      std::string::const_iterator cookie_value_beginning = i;
      if (*i == '"') {
        ++i;  // Skip '"'.
        while (i != header_value.end() && *i != '"') ++i;
        if (i == header_value.end()) return;
        ++i;  // Skip '"'.
        cookie_value = base::StringPiece(cookie_value_beginning, i);
        // i points to character after '"', potentially a ';'.
      } else {
        while (i != header_value.end() && *i != ';') ++i;
        cookie_value = base::StringPiece(cookie_value_beginning, i);
        // i points to ';' or end of string.
      }
    }
    parsed_cookies->emplace_back(cookie_name.as_string(),
                                 cookie_value.as_string());
    // Eat ';'.
    if (i != header_value.end()) ++i;
  }
}

std::string SerializeRequestCookieLine(
    const ParsedRequestCookies& parsed_cookies) {
  std::string buffer;
  for (auto i = parsed_cookies.begin(); i != parsed_cookies.end(); ++i) {
    if (!buffer.empty())
      buffer.append("; ");
    buffer.append(i->first.begin(), i->first.end());
    buffer.push_back('=');
    buffer.append(i->second.begin(), i->second.end());
  }
  return buffer;
}

CookieOptions::SameSiteCookieContext ComputeSameSiteContextForRequest(
    const std::string& http_method,
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& initiator,
    bool attach_same_site_cookies) {
  // Set SameSiteCookieMode according to the rules laid out in
  // https://tools.ietf.org/html/draft-ietf-httpbis-rfc6265bis-02:
  //
  // * Include both "strict" and "lax" same-site cookies if the request's
  //   |url|, |initiator|, and |site_for_cookies| all have the same
  //   registrable domain. Note: this also covers the case of a request
  //   without an initiator (only happens for browser-initiated main frame
  //   navigations).
  //
  // * Include only "lax" same-site cookies if the request's |URL| and
  //   |site_for_cookies| have the same registrable domain, _and_ the
  //   request's |http_method| is "safe" ("GET" or "HEAD").
  //
  //   This case should generally occur only for cross-site requests which
  //   target a top-level browsing context.
  //
  // * Include both "strict" and "lax" same-site cookies if the request is
  //   tagged with a flag allowing it.
  //
  //   Note that this can be the case for requests initiated by extensions,
  //   which need to behave as though they are made by the document itself,
  //   but appear like cross-site ones.
  //
  // * Otherwise, do not include same-site cookies.
  if (attach_same_site_cookies) {
    return ComputeSchemeChange(
        CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT, url,
        site_for_cookies);
  }

  CookieOptions::SameSiteCookieContext same_site_context =
      ComputeSameSiteContext(url, site_for_cookies, initiator);

  int scheme_bitmask =
      static_cast<int>(same_site_context) &
      (CookieOptions::kToSecureMask | CookieOptions::kToInsecureMask);

  // If the method is safe, the context is Lax. Otherwise, make a note that
  // the method is unsafe.
  if (CookieOptions::RemoveCrossSchemeBitmask(same_site_context) ==
          CookieOptions::SameSiteCookieContext::SAME_SITE_LAX &&
      !net::HttpUtil::IsMethodSafe(http_method)) {
    return CookieOptions::ApplyCrossSchemeBitmask(
        CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
        scheme_bitmask);
  }
  return same_site_context;
}

NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForScriptGet(const GURL& url,
                                   const GURL& site_for_cookies,
                                   const base::Optional<url::Origin>& initiator,
                                   bool attach_same_site_cookies) {
  if (attach_same_site_cookies) {
    return ComputeSchemeChange(
        CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT, url,
        site_for_cookies);
  }
  return ComputeSameSiteContext(url, site_for_cookies, initiator);
}

CookieOptions::SameSiteCookieContext ComputeSameSiteContextForResponse(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& initiator,
    bool attach_same_site_cookies) {
  // |initiator| is here in case it'll be decided to ignore |site_for_cookies|
  // for entirely browser-side requests (see https://crbug.com/958335).
  if (attach_same_site_cookies ||
      MatchesSiteForCookies(url, site_for_cookies)) {
    return ComputeSchemeChange(
        CookieOptions::SameSiteCookieContext::SAME_SITE_LAX, url,
        site_for_cookies);
  } else {
    return CookieOptions::SameSiteCookieContext::CROSS_SITE;
  }
}

CookieOptions::SameSiteCookieContext ComputeSameSiteContextForScriptSet(
    const GURL& url,
    const GURL& site_for_cookies,
    bool attach_same_site_cookies) {
  if (attach_same_site_cookies ||
      MatchesSiteForCookies(url, site_for_cookies)) {
    return ComputeSchemeChange(
        CookieOptions::SameSiteCookieContext::SAME_SITE_LAX, url,
        site_for_cookies);
  } else {
    return CookieOptions::SameSiteCookieContext::CROSS_SITE;
  }
}

CookieOptions::SameSiteCookieContext ComputeSameSiteContextForSubresource(
    const GURL& url,
    const GURL& site_for_cookies,
    bool attach_same_site_cookies) {
  // If the URL is same-site as site_for_cookies it's same-site as all frames
  // in the tree from the initiator frame up --- including the initiator frame.
  if (attach_same_site_cookies ||
      MatchesSiteForCookies(url, site_for_cookies)) {
    return ComputeSchemeChange(
        CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT, url,
        site_for_cookies);
  } else {
    return CookieOptions::SameSiteCookieContext::CROSS_SITE;
  }
}

bool IsSameSiteByDefaultCookiesEnabled() {
  return base::FeatureList::IsEnabled(features::kSameSiteByDefaultCookies);
}

bool IsCookiesWithoutSameSiteMustBeSecureEnabled() {
  return IsSameSiteByDefaultCookiesEnabled() &&
         base::FeatureList::IsEnabled(
             features::kCookiesWithoutSameSiteMustBeSecure);
}

base::OnceCallback<void(net::CanonicalCookie::CookieInclusionStatus)>
AdaptCookieInclusionStatusToBool(base::OnceCallback<void(bool)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(bool)> inner_callback,
         const net::CanonicalCookie::CookieInclusionStatus status) {
        bool success = status.IsInclude();
        std::move(inner_callback).Run(success);
      },
      std::move(callback));
}

CookieList StripStatuses(const CookieStatusList& cookie_status_list) {
  CookieList cookies;
  for (const CookieWithStatus& cookie_with_status : cookie_status_list) {
    cookies.push_back(cookie_with_status.cookie);
  }
  return cookies;
}

}  // namespace cookie_util
}  // namespace net
