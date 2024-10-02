// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/cookies/canonical_cookie.h"

#include <limits>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/features.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

using base::Time;

namespace net {

namespace {

static constexpr int kMinutesInTwelveHours = 12 * 60;
static constexpr int kMinutesInTwentyFourHours = 24 * 60;

// Determine the cookie domain to use for setting the specified cookie.
bool GetCookieDomain(const GURL& url,
                     const ParsedCookie& pc,
                     CookieInclusionStatus& status,
                     std::string* result) {
  std::string domain_string;
  if (pc.HasDomain())
    domain_string = pc.Domain();
  return cookie_util::GetCookieDomainWithString(url, domain_string, status,
                                                result);
}

// Compares cookies using name, domain and path, so that "equivalent" cookies
// (per RFC 2965) are equal to each other.
int PartialCookieOrdering(const CanonicalCookie& a, const CanonicalCookie& b) {
  int diff = a.Name().compare(b.Name());
  if (diff != 0)
    return diff;

  diff = a.Domain().compare(b.Domain());
  if (diff != 0)
    return diff;

  return a.Path().compare(b.Path());
}

void AppendCookieLineEntry(const CanonicalCookie& cookie,
                           std::string* cookie_line) {
  if (!cookie_line->empty())
    *cookie_line += "; ";
  // In Mozilla, if you set a cookie like "AAA", it will have an empty token
  // and a value of "AAA". When it sends the cookie back, it will send "AAA",
  // so we need to avoid sending "=AAA" for a blank token value.
  if (!cookie.Name().empty())
    *cookie_line += cookie.Name() + "=";
  *cookie_line += cookie.Value();
}

// Converts CookieSameSite to CookieSameSiteForMetrics by adding 1 to it.
CookieSameSiteForMetrics CookieSameSiteToCookieSameSiteForMetrics(
    CookieSameSite enum_in) {
  return static_cast<CookieSameSiteForMetrics>((static_cast<int>(enum_in) + 1));
}

auto GetAllDataMembersAsTuple(const CanonicalCookie& c) {
  return std::make_tuple(c.CreationDate(), c.LastAccessDate(), c.ExpiryDate(),
                         c.SecureAttribute(), c.IsHttpOnly(), c.SameSite(),
                         c.Priority(), c.PartitionKey(), c.Name(), c.Value(),
                         c.Domain(), c.Path(), c.LastUpdateDate(),
                         c.SourceScheme(), c.SourcePort(), c.SourceType());
}

}  // namespace

CookieAccessParams::CookieAccessParams(CookieAccessSemantics access_semantics,
                                       bool delegate_treats_url_as_trustworthy)
    : access_semantics(access_semantics),
      delegate_treats_url_as_trustworthy(delegate_treats_url_as_trustworthy) {}

CanonicalCookie::CanonicalCookie() = default;

CanonicalCookie::CanonicalCookie(const CanonicalCookie& other) = default;

CanonicalCookie::CanonicalCookie(CanonicalCookie&& other) = default;

CanonicalCookie& CanonicalCookie::operator=(const CanonicalCookie& other) =
    default;

CanonicalCookie& CanonicalCookie::operator=(CanonicalCookie&& other) = default;

CanonicalCookie::CanonicalCookie(
    base::PassKey<CanonicalCookie> pass_key,
    std::string name,
    std::string value,
    std::string domain,
    std::string path,
    base::Time creation,
    base::Time expiration,
    base::Time last_access,
    base::Time last_update,
    bool secure,
    bool httponly,
    CookieSameSite same_site,
    CookiePriority priority,
    std::optional<CookiePartitionKey> partition_key,
    CookieSourceScheme source_scheme,
    int source_port,
    CookieSourceType source_type)
    : CookieBase(std::move(name),
                 std::move(domain),
                 std::move(path),
                 creation,
                 secure,
                 httponly,
                 same_site,
                 std::move(partition_key),
                 source_scheme,
                 source_port),
      value_(std::move(value)),
      expiry_date_(expiration),
      last_access_date_(last_access),
      last_update_date_(last_update),
      priority_(priority),
      source_type_(source_type) {}

CanonicalCookie::~CanonicalCookie() = default;

// static
Time CanonicalCookie::ParseExpiration(const ParsedCookie& pc,
                                      const Time& current,
                                      const Time& server_time) {
  // First, try the Max-Age attribute.
  if (pc.HasMaxAge()) {
    int64_t max_age = 0;
    // Use the output if StringToInt64 returns true ("perfect" conversion). This
    // case excludes overflow/underflow, leading/trailing whitespace, non-number
    // strings, and empty string. (ParsedCookie trims whitespace.)
    if (base::StringToInt64(pc.MaxAge(), &max_age)) {
      // RFC 6265bis algorithm for parsing Max-Age:
      // "If delta-seconds is less than or equal to zero (0), let expiry-
      // time be the earliest representable date and time. ... "
      if (max_age <= 0)
        return Time::Min();
      // "... Otherwise, let the expiry-time be the current date and time plus
      // delta-seconds seconds."
      return current + base::Seconds(max_age);
    } else {
      // If the conversion wasn't perfect, but the best-effort conversion
      // resulted in an overflow/underflow, use the min/max representable time.
      // (This is alluded to in the spec, which says the user agent MAY clip an
      // Expires attribute to a saturated time. We'll do the same for Max-Age.)
      if (max_age == std::numeric_limits<int64_t>::min())
        return Time::Min();
      if (max_age == std::numeric_limits<int64_t>::max())
        return Time::Max();
    }
  }

  // Try the Expires attribute.
  if (pc.HasExpires() && !pc.Expires().empty()) {
    // Adjust for clock skew between server and host.
    Time parsed_expiry = cookie_util::ParseCookieExpirationTime(pc.Expires());
    if (!parsed_expiry.is_null()) {
      // Record metrics related to prevalence of clock skew.
      base::TimeDelta clock_skew = (current - server_time);
      // Record the magnitude (absolute value) of the skew in minutes.
      int clock_skew_magnitude = clock_skew.magnitude().InMinutes();
      // Determine the new expiry with clock skew factored in.
      Time adjusted_expiry = parsed_expiry + (current - server_time);
      if (clock_skew.is_positive() || clock_skew.is_zero()) {
        UMA_HISTOGRAM_CUSTOM_COUNTS("Cookie.ClockSkew.AddMinutes",
                                    clock_skew_magnitude, 1,
                                    kMinutesInTwelveHours, 100);
        UMA_HISTOGRAM_CUSTOM_COUNTS("Cookie.ClockSkew.AddMinutes12To24Hours",
                                    clock_skew_magnitude, kMinutesInTwelveHours,
                                    kMinutesInTwentyFourHours, 100);
        // Also record the range of minutes added that allowed the cookie to
        // avoid expiring immediately.
        if (parsed_expiry <= Time::Now() && adjusted_expiry > Time::Now()) {
          UMA_HISTOGRAM_CUSTOM_COUNTS(
              "Cookie.ClockSkew.WithoutAddMinutesExpires", clock_skew_magnitude,
              1, kMinutesInTwentyFourHours, 100);
        }
      } else if (clock_skew.is_negative()) {
        // These histograms only support positive numbers, so negative skews
        // will be converted to positive (via magnitude) before recording.
        UMA_HISTOGRAM_CUSTOM_COUNTS("Cookie.ClockSkew.SubtractMinutes",
                                    clock_skew_magnitude, 1,
                                    kMinutesInTwelveHours, 100);
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Cookie.ClockSkew.SubtractMinutes12To24Hours", clock_skew_magnitude,
            kMinutesInTwelveHours, kMinutesInTwentyFourHours, 100);
      }
      // Record if we were going to expire the cookie before we added the clock
      // skew.
      UMA_HISTOGRAM_BOOLEAN(
          "Cookie.ClockSkew.ExpiredWithoutSkew",
          parsed_expiry <= Time::Now() && adjusted_expiry > Time::Now());
      return adjusted_expiry;
    }
  }

  // Invalid or no expiration, session cookie.
  return Time();
}

// static
base::Time CanonicalCookie::ValidateAndAdjustExpiryDate(
    const base::Time& expiry_date,
    const base::Time& creation_date,
    net::CookieSourceScheme scheme) {
  if (expiry_date.is_null())
    return expiry_date;
  base::Time fixed_creation_date = creation_date;
  if (fixed_creation_date.is_null()) {
    // TODO(crbug.com/40800807): Push this logic into
    // CanonicalCookie::CreateSanitizedCookie. The four sites that call it
    // with a null `creation_date` (CanonicalCookie::Create cannot be called
    // this way) are:
    // * GaiaCookieManagerService::ForceOnCookieChangeProcessing
    // * CookiesSetFunction::Run
    // * cookie_store.cc::ToCanonicalCookie
    // * network_handler.cc::MakeCookieFromProtocolValues
    fixed_creation_date = base::Time::Now();
  }
  base::Time maximum_expiry_date;
  if (!cookie_util::IsTimeLimitedInsecureCookiesEnabled() ||
      scheme == net::CookieSourceScheme::kSecure) {
    maximum_expiry_date = fixed_creation_date + base::Days(400);
  } else {
    maximum_expiry_date = fixed_creation_date + base::Hours(3);
  }
  if (expiry_date > maximum_expiry_date) {
    return maximum_expiry_date;
  }
  return expiry_date;
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::Create(
    const GURL& url,
    const std::string& cookie_line,
    const base::Time& creation_time,
    std::optional<base::Time> server_time,
    std::optional<CookiePartitionKey> cookie_partition_key,
    CookieSourceType source_type,
    CookieInclusionStatus* status) {
  // Put a pointer on the stack so the rest of the function can assign to it if
  // the default nullptr is passed in.
  CookieInclusionStatus blank_status;
  if (status == nullptr) {
    status = &blank_status;
  }
  *status = CookieInclusionStatus();

  // Check the URL; it may be nonsense since some platform APIs may permit
  // it to be specified directly.
  if (!url.is_valid()) {
    status->AddExclusionReason(CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
    return nullptr;
  }

  ParsedCookie parsed_cookie(cookie_line, status);

  // We record this metric before checking validity because the presence of an
  // HTAB will invalidate the ParsedCookie.
  UMA_HISTOGRAM_BOOLEAN("Cookie.NameOrValueHtab",
                        parsed_cookie.HasInternalHtab());

  if (!parsed_cookie.IsValid()) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "WARNING: Couldn't parse cookie";
    DCHECK(!status->IsInclude());
    // Don't continue, because an invalid ParsedCookie doesn't have any
    // attributes.
    // TODO(chlily): Log metrics.
    return nullptr;
  }

  // Record warning for non-ASCII octecs in the Domain attribute.
  // This should lead to rejection of the cookie in the future.
  UMA_HISTOGRAM_BOOLEAN("Cookie.DomainHasNonASCII",
                        parsed_cookie.HasDomain() &&
                            !base::IsStringASCII(parsed_cookie.Domain()));

  std::string cookie_domain;
  if (!GetCookieDomain(url, parsed_cookie, *status, &cookie_domain)) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "Create() failed to get a valid cookie domain";
    status->AddExclusionReason(CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
  }

  std::string cookie_path = cookie_util::CanonPathWithString(
      url, parsed_cookie.HasPath() ? parsed_cookie.Path() : std::string());

  Time cookie_server_time(creation_time);
  if (server_time.has_value() && !server_time->is_null())
    cookie_server_time = server_time.value();

  DCHECK(!creation_time.is_null());

  CookiePrefix prefix = cookie_util::GetCookiePrefix(parsed_cookie.Name());

  bool is_cookie_prefix_valid =
      cookie_util::IsCookiePrefixValid(prefix, url, parsed_cookie);

  RecordCookiePrefixMetrics(prefix);

  if (parsed_cookie.Name() == "") {
    is_cookie_prefix_valid = !HasHiddenPrefixName(parsed_cookie.Value());
  }

  if (!is_cookie_prefix_valid) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "Create() failed because the cookie violated prefix rules.";
    status->AddExclusionReason(CookieInclusionStatus::EXCLUDE_INVALID_PREFIX);
  }

  bool partition_has_nonce = CookiePartitionKey::HasNonce(cookie_partition_key);
  bool is_partitioned_valid = cookie_util::IsCookiePartitionedValid(
      url, parsed_cookie, partition_has_nonce);
  if (!is_partitioned_valid) {
    status->AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_INVALID_PARTITIONED);
  }

  // Collect metrics on whether usage of the Partitioned attribute is correct.
  // Do not include implicit nonce-based partitioned cookies in these metrics.
  if (parsed_cookie.IsPartitioned()) {
    if (!partition_has_nonce)
      UMA_HISTOGRAM_BOOLEAN("Cookie.IsPartitionedValid", is_partitioned_valid);
  } else if (!partition_has_nonce) {
    cookie_partition_key = std::nullopt;
  }

  if (!status->IsInclude())
    return nullptr;

  CookieSameSiteString samesite_string = CookieSameSiteString::kUnspecified;
  CookieSameSite samesite = parsed_cookie.SameSite(&samesite_string);

  // The next two sections set the source_scheme_ and source_port_. Normally
  // these are taken directly from the url's scheme and port but if the url
  // setting this cookie is considered a trustworthy origin then we may make
  // some modifications. Note that here we assume that a trustworthy url must
  // have a non-secure scheme (http). Since we can't know at this point if a url
  // is trustworthy or not, we'll assume it is if the cookie is set with the
  // `Secure` attribute.
  //
  // For both convenience and to try to match expectations, cookies that have
  // the `Secure` attribute are modified to look like they were created by a
  // secure url. This is helpful because this cookie can be treated like any
  // other secure cookie when we're retrieving them and helps to prevent the
  // cookie from getting "trapped" if the url loses trustworthiness.

  CookieSourceScheme source_scheme;
  if (parsed_cookie.IsSecure() || url.SchemeIsCryptographic()) {
    // It's possible that a trustworthy origin is setting this cookie with the
    // `Secure` attribute even if the url's scheme isn't secure. In that case
    // we'll act like it was a secure scheme. This cookie will be rejected later
    // if the url isn't allowed to access secure cookies so this isn't a
    // problem.
    source_scheme = CookieSourceScheme::kSecure;

    if (!url.SchemeIsCryptographic()) {
      status->AddWarningReason(
          CookieInclusionStatus::
              WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME);
    }
  } else {
    source_scheme = CookieSourceScheme::kNonSecure;
  }

  // Get the port, this will get a default value if a port isn't explicitly
  // provided. Similar to the source scheme, it's possible that a trustworthy
  // origin is setting this cookie with the `Secure` attribute even if the url's
  // scheme isn't secure. This function will return 443 to pretend like this
  // cookie was set by a secure scheme.
  int source_port = CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
      url, parsed_cookie.IsSecure());

  Time cookie_expires = CanonicalCookie::ParseExpiration(
      parsed_cookie, creation_time, cookie_server_time);
  cookie_expires =
      ValidateAndAdjustExpiryDate(cookie_expires, creation_time, source_scheme);

  auto cc = std::make_unique<CanonicalCookie>(
      base::PassKey<CanonicalCookie>(), parsed_cookie.Name(),
      parsed_cookie.Value(), cookie_domain, cookie_path, creation_time,
      cookie_expires, creation_time,
      /*last_update=*/base::Time::Now(), parsed_cookie.IsSecure(),
      parsed_cookie.IsHttpOnly(), samesite, parsed_cookie.Priority(),
      cookie_partition_key, source_scheme, source_port, source_type);

  // TODO(chlily): Log metrics.
  if (!cc->IsCanonical()) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
    return nullptr;
  }

  RecordCookieSameSiteAttributeValueHistogram(samesite_string);

  // These metrics capture whether or not a cookie has a Non-ASCII character in
  // it.
  UMA_HISTOGRAM_BOOLEAN("Cookie.HasNonASCII.Name",
                        !base::IsStringASCII(cc->Name()));
  UMA_HISTOGRAM_BOOLEAN("Cookie.HasNonASCII.Value",
                        !base::IsStringASCII(cc->Value()));

  // Check for "__" prefixed names, excluding the cookie prefixes.
  bool name_prefixed_with_underscores =
      (prefix == COOKIE_PREFIX_NONE) && parsed_cookie.Name().starts_with("__");

  UMA_HISTOGRAM_BOOLEAN("Cookie.DoubleUnderscorePrefixedName",
                        name_prefixed_with_underscores);

  return cc;
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::CreateSanitizedCookie(
    const GURL& url,
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    base::Time creation_time,
    base::Time expiration_time,
    base::Time last_access_time,
    bool secure,
    bool http_only,
    CookieSameSite same_site,
    CookiePriority priority,
    std::optional<CookiePartitionKey> partition_key,
    CookieInclusionStatus* status) {
  // Put a pointer on the stack so the rest of the function can assign to it if
  // the default nullptr is passed in.
  CookieInclusionStatus blank_status;
  if (status == nullptr) {
    status = &blank_status;
  }
  *status = CookieInclusionStatus();

  // Validate consistency of passed arguments.
  if (ParsedCookie::ParseTokenString(name) != name) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER);
  } else if (ParsedCookie::ParseValueString(value) != value) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER);
  } else if (ParsedCookie::ParseValueString(path) != path) {
    // NOTE: If `path` contains  "terminating characters" ('\r', '\n', and
    // '\0'), ';', or leading / trailing whitespace, path will be rejected,
    // but any other control characters will just get URL-encoded below.
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER);
  }

  // Validate name and value against character set and size limit constraints.
  // If IsValidCookieNameValuePair identifies that `name` and/or `value` are
  // invalid, it will add an ExclusionReason to `status`.
  ParsedCookie::IsValidCookieNameValuePair(name, value, status);

  // Validate domain against character set and size limit constraints.
  bool domain_is_valid = true;

  if ((ParsedCookie::ParseValueString(domain) != domain)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
    domain_is_valid = false;
  }

  if (!ParsedCookie::CookieAttributeValueHasValidCharSet(domain)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
    domain_is_valid = false;
  }
  if (!ParsedCookie::CookieAttributeValueHasValidSize(domain)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE);
    domain_is_valid = false;
  }
  const std::string& domain_attribute =
      domain_is_valid ? domain : std::string();

  std::string cookie_domain;
  // This validation step must happen before GetCookieDomainWithString, so it
  // doesn't fail DCHECKs.
  if (!cookie_util::DomainIsHostOnly(url.host())) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
  } else if (!cookie_util::GetCookieDomainWithString(url, domain_attribute,
                                                     *status, &cookie_domain)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
  }

  // The next two sections set the source_scheme_ and source_port_. Normally
  // these are taken directly from the url's scheme and port but if the url
  // setting this cookie is considered a trustworthy origin then we may make
  // some modifications. Note that here we assume that a trustworthy url must
  // have a non-secure scheme (http). Since we can't know at this point if a url
  // is trustworthy or not, we'll assume it is if the cookie is set with the
  // `Secure` attribute.
  //
  // For both convenience and to try to match expectations, cookies that have
  // the `Secure` attribute are modified to look like they were created by a
  // secure url. This is helpful because this cookie can be treated like any
  // other secure cookie when we're retrieving them and helps to prevent the
  // cookie from getting "trapped" if the url loses trustworthiness.

  CookieSourceScheme source_scheme = CookieSourceScheme::kNonSecure;
  // This validation step must happen before SchemeIsCryptographic, so it
  // doesn't fail DCHECKs.
  if (!url.is_valid()) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
  } else {
    // It's possible that a trustworthy origin is setting this cookie with the
    // `Secure` attribute even if the url's scheme isn't secure. In that case
    // we'll act like it was a secure scheme. This cookie will be rejected later
    // if the url isn't allowed to access secure cookies so this isn't a
    // problem.
    source_scheme = (secure || url.SchemeIsCryptographic())
                        ? CookieSourceScheme::kSecure
                        : CookieSourceScheme::kNonSecure;

    if (source_scheme == CookieSourceScheme::kSecure &&
        !url.SchemeIsCryptographic()) {
      status->AddWarningReason(
          CookieInclusionStatus::
              WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME);
    }
  }

  // Get the port, this will get a default value if a port isn't explicitly
  // provided. Similar to the source scheme, it's possible that a trustworthy
  // origin is setting this cookie with the `Secure` attribute even if the url's
  // scheme isn't secure. This function will return 443 to pretend like this
  // cookie was set by a secure scheme.
  int source_port =
      CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(url, secure);

  std::string cookie_path = cookie_util::CanonPathWithString(url, path);
  // Canonicalize path again to make sure it escapes characters as needed.
  url::Component path_component(0, cookie_path.length());
  url::RawCanonOutputT<char> canon_path;
  url::Component canon_path_component;
  url::CanonicalizePath(cookie_path.data(), path_component, &canon_path,
                        &canon_path_component);
  std::string encoded_cookie_path = std::string(
      canon_path.data() + canon_path_component.begin, canon_path_component.len);

  if (!path.empty()) {
    if (cookie_path != path) {
      // The path attribute was specified and found to be invalid, so record an
      // error.
      status->AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
    } else if (!ParsedCookie::CookieAttributeValueHasValidSize(
                   encoded_cookie_path)) {
      // The path attribute was specified and encodes into a value that's longer
      // than the length limit, so record an error.
      status->AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE);
    }
  }

  CookiePrefix prefix = cookie_util::GetCookiePrefix(name);
  if (!cookie_util::IsCookiePrefixValid(prefix, url, secure, domain_attribute,
                                        cookie_path)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX);
  }

  if (name == "" && HasHiddenPrefixName(value)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX);
  }

  if (!cookie_util::IsCookiePartitionedValid(
          url, secure,
          /*is_partitioned=*/partition_key.has_value(),
          /*partition_has_nonce=*/
          CookiePartitionKey::HasNonce(partition_key))) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_PARTITIONED);
  }

  if (!last_access_time.is_null() && creation_time.is_null()) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
  }
  expiration_time = ValidateAndAdjustExpiryDate(expiration_time, creation_time,
                                                source_scheme);

  if (!status->IsInclude())
    return nullptr;

  auto cc = std::make_unique<CanonicalCookie>(
      base::PassKey<CanonicalCookie>(), name, value, cookie_domain,
      encoded_cookie_path, creation_time, expiration_time, last_access_time,
      /*last_update=*/base::Time::Now(), secure, http_only, same_site, priority,
      partition_key, source_scheme, source_port, CookieSourceType::kOther);
  DCHECK(cc->IsCanonical());

  return cc;
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::FromStorage(
    std::string name,
    std::string value,
    std::string domain,
    std::string path,
    base::Time creation,
    base::Time expiration,
    base::Time last_access,
    base::Time last_update,
    bool secure,
    bool httponly,
    CookieSameSite same_site,
    CookiePriority priority,
    std::optional<CookiePartitionKey> partition_key,
    CookieSourceScheme source_scheme,
    int source_port,
    CookieSourceType source_type) {
  // We check source_port here because it could have concievably been
  // corrupted and changed to out of range. Eventually this would be caught by
  // IsCanonical*() but since the source_port is only used by metrics so far
  // nothing else checks it. So let's normalize it here and then update this
  // method when origin-bound cookies is implemented.
  // TODO(crbug.com/40165805)
  int validated_port = CookieBase::ValidateAndAdjustSourcePort(source_port);

  auto cc = std::make_unique<CanonicalCookie>(
      base::PassKey<CanonicalCookie>(), std::move(name), std::move(value),
      std::move(domain), std::move(path), creation, expiration, last_access,
      last_update, secure, httponly, same_site, priority, partition_key,
      source_scheme, validated_port, source_type);

  if (cc->IsCanonicalForFromStorage()) {
    // This will help capture the number of times a cookie is canonical but does
    // not have a valid name+value size length
    bool valid_cookie_name_value_pair =
        ParsedCookie::IsValidCookieNameValuePair(cc->Name(), cc->Value());
    UMA_HISTOGRAM_BOOLEAN("Cookie.FromStorageWithValidLength",
                          valid_cookie_name_value_pair);
  } else {
    return nullptr;
  }
  return cc;
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::CreateUnsafeCookieForTesting(
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    const base::Time& creation,
    const base::Time& expiration,
    const base::Time& last_access,
    const base::Time& last_update,
    bool secure,
    bool httponly,
    CookieSameSite same_site,
    CookiePriority priority,
    std::optional<CookiePartitionKey> partition_key,
    CookieSourceScheme source_scheme,
    int source_port,
    CookieSourceType source_type) {
  return std::make_unique<CanonicalCookie>(
      base::PassKey<CanonicalCookie>(), name, value, domain, path, creation,
      expiration, last_access, last_update, secure, httponly, same_site,
      priority, partition_key, source_scheme, source_port, source_type);
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::CreateForTesting(
    const GURL& url,
    const std::string& cookie_line,
    const base::Time& creation_time,
    std::optional<base::Time> server_time,
    std::optional<CookiePartitionKey> cookie_partition_key,
    CookieSourceType source_type,
    CookieInclusionStatus* status) {
  return CanonicalCookie::Create(url, cookie_line, creation_time, server_time,
                                 cookie_partition_key, source_type, status);
}

bool CanonicalCookie::IsEquivalentForSecureCookieMatching(
    const CanonicalCookie& secure_cookie) const {
  // Partition keys must both be equivalent.
  bool same_partition_key = PartitionKey() == secure_cookie.PartitionKey();

  // Names must be the same
  bool same_name = Name() == secure_cookie.Name();

  // They should domain-match in one direction or the other. (See RFC 6265bis
  // section 5.1.3.)
  // TODO(chlily): This does not check for the IP address case. This is bad due
  // to https://crbug.com/1069935.
  bool domain_match =
      IsSubdomainOf(DomainWithoutDot(), secure_cookie.DomainWithoutDot()) ||
      IsSubdomainOf(secure_cookie.DomainWithoutDot(), DomainWithoutDot());

  bool path_match = secure_cookie.IsOnPath(Path());

  bool equivalent_for_secure_cookie_matching =
      same_partition_key && same_name && domain_match && path_match;

  // IsEquivalent() is a stricter check than this.
  DCHECK(!IsEquivalent(secure_cookie) || equivalent_for_secure_cookie_matching);

  return equivalent_for_secure_cookie_matching;
}

bool CanonicalCookie::HasEquivalentDataMembers(
    const CanonicalCookie& other) const {
  return GetAllDataMembersAsTuple(*this) == GetAllDataMembersAsTuple(other);
}

void CanonicalCookie::PostIncludeForRequestURL(
    const CookieAccessResult& access_result,
    const CookieOptions& options_used,
    CookieOptions::SameSiteCookieContext::ContextType
        cookie_inclusion_context_used) const {
  UMA_HISTOGRAM_ENUMERATION(
      "Cookie.RequestSameSiteContext", cookie_inclusion_context_used,
      CookieOptions::SameSiteCookieContext::ContextType::COUNT);

  // For the metric, we only want to consider first party partitioned cookies.
  if (IsFirstPartyPartitioned()) {
    UMA_HISTOGRAM_BOOLEAN(
        "Cookie.FirstPartyPartitioned.HasCrossSiteAncestor",
        cookie_inclusion_context_used ==
            CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE);
  }

  if (access_result.status.IsInclude()) {
    UMA_HISTOGRAM_ENUMERATION("Cookie.IncludedRequestEffectiveSameSite",
                              access_result.effective_same_site,
                              CookieEffectiveSameSite::COUNT);
  }

  using ContextRedirectTypeBug1221316 = CookieOptions::SameSiteCookieContext::
      ContextMetadata::ContextRedirectTypeBug1221316;

  ContextRedirectTypeBug1221316 redirect_type_for_metrics =
      options_used.same_site_cookie_context()
          .GetMetadataForCurrentSchemefulMode()
          .redirect_type_bug_1221316;
  if (redirect_type_for_metrics != ContextRedirectTypeBug1221316::kUnset) {
    UMA_HISTOGRAM_ENUMERATION("Cookie.CrossSiteRedirectType.Read",
                              redirect_type_for_metrics);
  }

  if (access_result.status.HasWarningReason(
          CookieInclusionStatus::
              WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Cookie.CrossSiteRedirectDowngradeChangesInclusion2.Read",
        CookieSameSiteToCookieSameSiteForMetrics(SameSite()));

    using HttpMethod =
        CookieOptions::SameSiteCookieContext::ContextMetadata::HttpMethod;

    HttpMethod http_method_enum = options_used.same_site_cookie_context()
                                      .GetMetadataForCurrentSchemefulMode()
                                      .http_method_bug_1221316;

    DCHECK(http_method_enum != HttpMethod::kUnset);

    UMA_HISTOGRAM_ENUMERATION(
        "Cookie.CrossSiteRedirectDowngradeChangesInclusionHttpMethod",
        http_method_enum);

    base::TimeDelta cookie_age = base::Time::Now() - CreationDate();
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Cookie.CrossSiteRedirectDowngradeChangesInclusionAge",
        cookie_age.InMinutes(), 30);
  }
}

void CanonicalCookie::PostIsSetPermittedInContext(
    const CookieAccessResult& access_result,
    const CookieOptions& options_used) const {
  if (access_result.status.IsInclude()) {
    UMA_HISTOGRAM_ENUMERATION("Cookie.IncludedResponseEffectiveSameSite",
                              access_result.effective_same_site,
                              CookieEffectiveSameSite::COUNT);
  }

  using ContextRedirectTypeBug1221316 = CookieOptions::SameSiteCookieContext::
      ContextMetadata::ContextRedirectTypeBug1221316;

  ContextRedirectTypeBug1221316 redirect_type_for_metrics =
      options_used.same_site_cookie_context()
          .GetMetadataForCurrentSchemefulMode()
          .redirect_type_bug_1221316;
  if (redirect_type_for_metrics != ContextRedirectTypeBug1221316::kUnset) {
    UMA_HISTOGRAM_ENUMERATION("Cookie.CrossSiteRedirectType.Write",
                              redirect_type_for_metrics);
  }

  if (access_result.status.HasWarningReason(
          CookieInclusionStatus::
              WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Cookie.CrossSiteRedirectDowngradeChangesInclusion2.Write",
        CookieSameSiteToCookieSameSiteForMetrics(SameSite()));
  }
}

base::TimeDelta CanonicalCookie::GetLaxAllowUnsafeThresholdAge() const {
  return base::FeatureList::IsEnabled(
             features::kSameSiteDefaultChecksMethodRigorously)
             ? base::TimeDelta::Min()
             : (base::FeatureList::IsEnabled(
                    features::kShortLaxAllowUnsafeThreshold)
                    ? kShortLaxAllowUnsafeMaxAge
                    : kLaxAllowUnsafeMaxAge);
}

std::string CanonicalCookie::DebugString() const {
  return base::StringPrintf(
      "name: %s value: %s domain: %s path: %s creation: %" PRId64,
      Name().c_str(), value_.c_str(), Domain().c_str(), Path().c_str(),
      static_cast<int64_t>(CreationDate().ToTimeT()));
}

bool CanonicalCookie::PartialCompare(const CanonicalCookie& other) const {
  return PartialCookieOrdering(*this, other) < 0;
}

bool CanonicalCookie::IsCanonical() const {
  // TODO(crbug.com/40787717) Eventually we should check the size of name+value,
  // assuming we collect metrics and determine that a low percentage of cookies
  // would fail this check. Note that we still don't want to enforce length
  // checks on domain or path for the reason stated above.

  // TODO(crbug.com/40800807): Eventually we should push this logic into
  // IsCanonicalForFromStorage, but for now we allow cookies already stored with
  // high expiration dates to be retrieved.
  if (ValidateAndAdjustExpiryDate(expiry_date_, CreationDate(),
                                  SourceScheme()) != expiry_date_) {
    return false;
  }

  return IsCanonicalForFromStorage();
}

bool CanonicalCookie::IsCanonicalForFromStorage() const {
  // Not checking domain or path against ParsedCookie as it may have
  // come purely from the URL. Also, don't call IsValidCookieNameValuePair()
  // here because we don't want to enforce the size checks on names or values
  // that may have been reconstituted from the cookie store.
  if (ParsedCookie::ParseTokenString(Name()) != Name() ||
      !ParsedCookie::ValueMatchesParsedValue(value_)) {
    return false;
  }

  if (!ParsedCookie::IsValidCookieName(Name()) ||
      !ParsedCookie::IsValidCookieValue(value_)) {
    return false;
  }

  if (!last_access_date_.is_null() && CreationDate().is_null()) {
    return false;
  }

  url::CanonHostInfo canon_host_info;
  std::string canonical_domain(CanonicalizeHost(Domain(), &canon_host_info));

  // TODO(rdsmith): This specifically allows for empty domains.  The spec
  // suggests this is invalid (if a domain attribute is empty, the cookie's
  // domain is set to the canonicalized request host; see
  // https://tools.ietf.org/html/rfc6265#section-5.3).  However, it is
  // needed for Chrome extension cookies.
  // Note: The above comment may be outdated. We should determine whether empty
  // Domain() is ever valid and update this code accordingly.
  // See http://crbug.com/730633 for more information.
  if (canonical_domain != Domain()) {
    return false;
  }

  if (Path().empty() || Path()[0] != '/') {
    return false;
  }

  CookiePrefix prefix = cookie_util::GetCookiePrefix(Name());
  switch (prefix) {
    case COOKIE_PREFIX_HOST:
      if (!SecureAttribute() || Path() != "/" || Domain().empty() ||
          Domain()[0] == '.') {
        return false;
      }
      break;
    case COOKIE_PREFIX_SECURE:
      if (!SecureAttribute()) {
        return false;
      }
      break;
    default:
      break;
  }

  if (Name() == "" && HasHiddenPrefixName(value_)) {
    return false;
  }

  if (IsPartitioned()) {
    if (CookiePartitionKey::HasNonce(PartitionKey())) {
      return true;
    }
    if (!SecureAttribute()) {
      return false;
    }
  }

  return true;
}

bool CanonicalCookie::IsEffectivelySameSiteNone(
    CookieAccessSemantics access_semantics) const {
  return GetEffectiveSameSite(access_semantics) ==
         CookieEffectiveSameSite::NO_RESTRICTION;
}

CookieEffectiveSameSite CanonicalCookie::GetEffectiveSameSiteForTesting(
    CookieAccessSemantics access_semantics) const {
  return GetEffectiveSameSite(access_semantics);
}

// static
std::string CanonicalCookie::BuildCookieLine(const CookieList& cookies) {
  std::string cookie_line;
  for (const auto& cookie : cookies) {
    AppendCookieLineEntry(cookie, &cookie_line);
  }
  return cookie_line;
}

// static
std::string CanonicalCookie::BuildCookieLine(
    const CookieAccessResultList& cookie_access_result_list) {
  std::string cookie_line;
  for (const auto& cookie_with_access_result : cookie_access_result_list) {
    const CanonicalCookie& cookie = cookie_with_access_result.cookie;
    AppendCookieLineEntry(cookie, &cookie_line);
  }
  return cookie_line;
}

// static
std::string CanonicalCookie::BuildCookieAttributesLine(
    const CanonicalCookie& cookie) {
  std::string cookie_line;
  // In Mozilla, if you set a cookie like "AAA", it will have an empty token
  // and a value of "AAA". When it sends the cookie back, it will send "AAA",
  // so we need to avoid sending "=AAA" for a blank token value.
  if (!cookie.Name().empty())
    cookie_line += cookie.Name() + "=";
  cookie_line += cookie.Value();
  if (!cookie.Domain().empty())
    cookie_line += "; domain=" + cookie.Domain();
  if (!cookie.Path().empty())
    cookie_line += "; path=" + cookie.Path();
  if (cookie.ExpiryDate() != base::Time())
    cookie_line += "; expires=" + HttpUtil::TimeFormatHTTP(cookie.ExpiryDate());
  if (cookie.SecureAttribute()) {
    cookie_line += "; secure";
  }
  if (cookie.IsHttpOnly())
    cookie_line += "; httponly";
  if (cookie.IsPartitioned() &&
      !CookiePartitionKey::HasNonce(cookie.PartitionKey())) {
    cookie_line += "; partitioned";
  }
  switch (cookie.SameSite()) {
    case CookieSameSite::NO_RESTRICTION:
      cookie_line += "; samesite=none";
      break;
    case CookieSameSite::LAX_MODE:
      cookie_line += "; samesite=lax";
      break;
    case CookieSameSite::STRICT_MODE:
      cookie_line += "; samesite=strict";
      break;
    case CookieSameSite::UNSPECIFIED:
      // Don't append any text if the samesite attribute wasn't explicitly set.
      break;
  }
  return cookie_line;
}

// static
void CanonicalCookie::RecordCookiePrefixMetrics(CookiePrefix prefix) {
  const char kCookiePrefixHistogram[] = "Cookie.CookiePrefix";
  UMA_HISTOGRAM_ENUMERATION(kCookiePrefixHistogram, prefix, COOKIE_PREFIX_LAST);
}

// static
int CanonicalCookie::GetAndAdjustPortForTrustworthyUrls(
    const GURL& source_url,
    bool url_is_trustworthy) {
  // If the url isn't trustworthy, or if `source_url` is cryptographic then
  // return the port of `source_url`.
  if (!url_is_trustworthy || source_url.SchemeIsCryptographic()) {
    return source_url.EffectiveIntPort();
  }

  // Only http and ws are cookieable schemes that have a port component. For
  // both of these schemes their default port is 80 whereas their secure
  // components have a default port of 443.
  //
  // Only in cases where we have an http/ws scheme with a default should we
  // return 443.
  if ((source_url.SchemeIs(url::kHttpScheme) ||
       source_url.SchemeIs(url::kWsScheme)) &&
      source_url.EffectiveIntPort() == 80) {
    return 443;
  }

  // Different schemes, or non-default port values should keep the same port
  // value.
  return source_url.EffectiveIntPort();
}

// static
bool CanonicalCookie::HasHiddenPrefixName(std::string_view cookie_value) {
  // Skip BWS as defined by HTTPSEM as SP or HTAB (0x20 or 0x9).
  std::string_view value_without_BWS =
      base::TrimString(cookie_value, " \t", base::TRIM_LEADING);

  const std::string_view host_prefix = "__Host-";

  // Compare the value to the host_prefix.
  if (base::StartsWith(value_without_BWS, host_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // This value contains a hidden prefix name.
    return true;
  }

  // Do a similar check for the secure prefix
  const std::string_view secure_prefix = "__Secure-";

  if (base::StartsWith(value_without_BWS, secure_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return true;
  }

  return false;
}

CookieAndLineWithAccessResult::CookieAndLineWithAccessResult() = default;

CookieAndLineWithAccessResult::CookieAndLineWithAccessResult(
    std::optional<CanonicalCookie> cookie,
    std::string cookie_string,
    CookieAccessResult access_result)
    : cookie(std::move(cookie)),
      cookie_string(std::move(cookie_string)),
      access_result(access_result) {}

CookieAndLineWithAccessResult::CookieAndLineWithAccessResult(
    const CookieAndLineWithAccessResult&) = default;

CookieAndLineWithAccessResult& CookieAndLineWithAccessResult::operator=(
    const CookieAndLineWithAccessResult& cookie_and_line_with_access_result) =
    default;

CookieAndLineWithAccessResult::CookieAndLineWithAccessResult(
    CookieAndLineWithAccessResult&&) = default;

CookieAndLineWithAccessResult::~CookieAndLineWithAccessResult() = default;

}  // namespace net
