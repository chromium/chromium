// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_constants.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"

namespace net {

const base::TimeDelta kLaxAllowUnsafeMaxAge = base::TimeDelta::FromMinutes(2);
const base::TimeDelta kShortLaxAllowUnsafeMaxAge =
    base::TimeDelta::FromSeconds(10);

namespace {

const char kPriorityLow[] = "low";
const char kPriorityMedium[] = "medium";
const char kPriorityHigh[] = "high";

const char kSameSiteLax[] = "lax";
const char kSameSiteStrict[] = "strict";
const char kSameSiteNone[] = "none";
const char kSameSiteExtended[] = "extended";
const char kSameSiteUnspecified[] = "unspecified";

}  // namespace

std::string CookiePriorityToString(CookiePriority priority) {
  switch(priority) {
    case COOKIE_PRIORITY_HIGH:
      return kPriorityHigh;
    case COOKIE_PRIORITY_MEDIUM:
      return kPriorityMedium;
    case COOKIE_PRIORITY_LOW:
      return kPriorityLow;
    default:
      NOTREACHED();
  }
  return std::string();
}

CookiePriority StringToCookiePriority(const std::string& priority) {
  std::string priority_comp = base::ToLowerASCII(priority);

  if (priority_comp == kPriorityHigh)
    return COOKIE_PRIORITY_HIGH;
  if (priority_comp == kPriorityMedium)
    return COOKIE_PRIORITY_MEDIUM;
  if (priority_comp == kPriorityLow)
    return COOKIE_PRIORITY_LOW;

  return COOKIE_PRIORITY_DEFAULT;
}

std::string CookieSameSiteToString(CookieSameSite same_site) {
  switch (same_site) {
    case CookieSameSite::LAX_MODE:
      return kSameSiteLax;
    case CookieSameSite::STRICT_MODE:
      return kSameSiteStrict;
    case CookieSameSite::NO_RESTRICTION:
      return kSameSiteNone;
    case CookieSameSite::UNSPECIFIED:
      return kSameSiteUnspecified;
  }
}

CookieSameSite StringToCookieSameSite(const std::string& same_site,
                                      CookieSameSiteString* samesite_string) {
  // Put a value on the stack so that we can assign to |*samesite_string|
  // instead of having to null-check it all the time.
  CookieSameSiteString ignored = CookieSameSiteString::kUnspecified;
  if (!samesite_string)
    samesite_string = &ignored;

  *samesite_string = CookieSameSiteString::kUnrecognized;
  CookieSameSite samesite = CookieSameSite::UNSPECIFIED;

  if (base::EqualsCaseInsensitiveASCII(same_site, kSameSiteNone)) {
    samesite = CookieSameSite::NO_RESTRICTION;
    *samesite_string = CookieSameSiteString::kNone;
  } else if (base::EqualsCaseInsensitiveASCII(same_site, kSameSiteLax)) {
    samesite = CookieSameSite::LAX_MODE;
    *samesite_string = CookieSameSiteString::kLax;
  } else if (base::EqualsCaseInsensitiveASCII(same_site, kSameSiteStrict)) {
    samesite = CookieSameSite::STRICT_MODE;
    *samesite_string = CookieSameSiteString::kStrict;
  } else if (base::EqualsCaseInsensitiveASCII(same_site, kSameSiteExtended)) {
    // Extended isn't supported anymore -- we just parse it for UMA stats.
    *samesite_string = CookieSameSiteString::kExtended;
  } else if (same_site == "") {
    *samesite_string = CookieSameSiteString::kEmptyString;
  }
  return samesite;
}

void RecordCookieSameSiteAttributeValueHistogram(CookieSameSiteString value) {
  UMA_HISTOGRAM_ENUMERATION("Cookie.SameSiteAttributeValue", value);
}

}  // namespace net
