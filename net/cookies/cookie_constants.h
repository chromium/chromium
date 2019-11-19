// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_CONSTANTS_H_
#define NET_COOKIES_COOKIE_CONSTANTS_H_

#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

// The time threshold for considering a cookie "short-lived" for the purposes of
// allowing unsafe methods for unspecified-SameSite cookies defaulted into Lax.
NET_EXPORT extern const base::TimeDelta kLaxAllowUnsafeMaxAge;
// The short version of the above time threshold, to be used for tests.
NET_EXPORT extern const base::TimeDelta kShortLaxAllowUnsafeMaxAge;

enum CookiePriority {
  COOKIE_PRIORITY_LOW     = 0,
  COOKIE_PRIORITY_MEDIUM  = 1,
  COOKIE_PRIORITY_HIGH    = 2,
  COOKIE_PRIORITY_DEFAULT = COOKIE_PRIORITY_MEDIUM
};

// See https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00
// and https://tools.ietf.org/html/draft-ietf-httpbis-rfc6265bis for
// information about same site cookie restrictions.
// These values are allowed for the SameSite field of a cookie. They mostly
// correspond to CookieEffectiveSameSite values.
// Note: Don't renumber, as these values are persisted to a database.
enum class CookieSameSite {
  UNSPECIFIED = -1,
  NO_RESTRICTION = 0,
  LAX_MODE = 1,
  STRICT_MODE = 2,
  // Reserved 3 (was EXTENDED_MODE), next number is 4.
};

// These are the enforcement modes that may be applied to a cookie when deciding
// inclusion/exclusion. They mostly correspond to CookieSameSite values.
// Keep in sync with enums.xml.
enum class CookieEffectiveSameSite {
  NO_RESTRICTION = 0,
  LAX_MODE = 1,
  STRICT_MODE = 2,
  LAX_MODE_ALLOW_UNSAFE = 3,

  // Keep last, used for histograms.
  COUNT
};

// Used for histograms only. Do not renumber. Keep in sync with enums.xml.
enum class CookieSameSiteString {
  // No SameSite attribute is present.
  kUnspecified = 0,
  // The SameSite attribute is present but has no value.
  kEmptyString = 1,
  // The SameSite attribute has an unrecognized value.
  kUnrecognized = 2,
  // The SameSite attribute has a recognized value.
  kLax = 3,
  kStrict = 4,
  kNone = 5,
  kExtended = 6,  // Deprecated, kept for metrics only.

  // Keep last, update if adding new value.
  kMaxValue = kExtended
};

// What rules to apply when determining whether access to a particular cookie is
// allowed.
enum class CookieAccessSemantics {
  // Has not been checked yet or there is no way to check.
  UNKNOWN = -1,
  // Has been checked and the cookie should *not* be subject to legacy access
  // rules.
  NONLEGACY = 0,
  // Has been checked and the cookie should be subject to legacy access rules.
  LEGACY,
};

// Returns the Set-Cookie header priority token corresponding to |priority|.
//
// TODO(mkwst): Remove this once its callsites are refactored.
NET_EXPORT std::string CookiePriorityToString(CookiePriority priority);

// Converts the Set-Cookie header priority token |priority| to a CookiePriority.
// Defaults to COOKIE_PRIORITY_DEFAULT for empty or unrecognized strings.
NET_EXPORT CookiePriority StringToCookiePriority(const std::string& priority);

// Returns a string corresponding to the value of the |same_site| token.
// Intended only for debugging/logging.
NET_EXPORT std::string CookieSameSiteToString(CookieSameSite same_site);

// Converts the Set-Cookie header SameSite token |same_site| to a
// CookieSameSite. Defaults to CookieSameSite::UNSPECIFIED for empty or
// unrecognized strings. Returns an appropriate value of CookieSameSiteString in
// |samesite_string| to indicate what type of string was parsed as the SameSite
// attribute value, if a pointer is provided.
NET_EXPORT CookieSameSite
StringToCookieSameSite(const std::string& same_site,
                       CookieSameSiteString* samesite_string = nullptr);

NET_EXPORT void RecordCookieSameSiteAttributeValueHistogram(
    CookieSameSiteString value);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_CONSTANTS_H_
