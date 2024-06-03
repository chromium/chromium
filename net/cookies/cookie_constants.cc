// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_constants.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "url/url_constants.h"

namespace net {

const base::TimeDelta kLaxAllowUnsafeMaxAge = base::Minutes(2);
const base::TimeDelta kShortLaxAllowUnsafeMaxAge = base::Seconds(10);

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
      NOTREACHED_IN_MIGRATION();
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

CookiePort ReducePortRangeForCookieHistogram(const int port) {
  switch (port) {
    case 80:
      return CookiePort::k80;
    case 81:
      return CookiePort::k81;
    case 82:
      return CookiePort::k82;
    case 83:
      return CookiePort::k83;
    case 84:
      return CookiePort::k84;
    case 85:
      return CookiePort::k85;
    case 443:
      return CookiePort::k443;
    case 444:
      return CookiePort::k444;
    case 445:
      return CookiePort::k445;
    case 446:
      return CookiePort::k446;
    case 447:
      return CookiePort::k447;
    case 448:
      return CookiePort::k448;
    case 3000:
      return CookiePort::k3000;
    case 3001:
      return CookiePort::k3001;
    case 3002:
      return CookiePort::k3002;
    case 3003:
      return CookiePort::k3003;
    case 3004:
      return CookiePort::k3004;
    case 3005:
      return CookiePort::k3005;
    case 4200:
      return CookiePort::k4200;
    case 4201:
      return CookiePort::k4201;
    case 4202:
      return CookiePort::k4202;
    case 4203:
      return CookiePort::k4203;
    case 4204:
      return CookiePort::k4204;
    case 4205:
      return CookiePort::k4205;
    case 5000:
      return CookiePort::k5000;
    case 5001:
      return CookiePort::k5001;
    case 5002:
      return CookiePort::k5002;
    case 5003:
      return CookiePort::k5003;
    case 5004:
      return CookiePort::k5004;
    case 5005:
      return CookiePort::k5005;
    case 7000:
      return CookiePort::k7000;
    case 7001:
      return CookiePort::k7001;
    case 7002:
      return CookiePort::k7002;
    case 7003:
      return CookiePort::k7003;
    case 7004:
      return CookiePort::k7004;
    case 7005:
      return CookiePort::k7005;
    case 8000:
      return CookiePort::k8000;
    case 8001:
      return CookiePort::k8001;
    case 8002:
      return CookiePort::k8002;
    case 8003:
      return CookiePort::k8003;
    case 8004:
      return CookiePort::k8004;
    case 8005:
      return CookiePort::k8005;
    case 8080:
      return CookiePort::k8080;
    case 8081:
      return CookiePort::k8081;
    case 8082:
      return CookiePort::k8082;
    case 8083:
      return CookiePort::k8083;
    case 8084:
      return CookiePort::k8084;
    case 8085:
      return CookiePort::k8085;
    case 8090:
      return CookiePort::k8090;
    case 8091:
      return CookiePort::k8091;
    case 8092:
      return CookiePort::k8092;
    case 8093:
      return CookiePort::k8093;
    case 8094:
      return CookiePort::k8094;
    case 8095:
      return CookiePort::k8095;
    case 8100:
      return CookiePort::k8100;
    case 8101:
      return CookiePort::k8101;
    case 8102:
      return CookiePort::k8102;
    case 8103:
      return CookiePort::k8103;
    case 8104:
      return CookiePort::k8104;
    case 8105:
      return CookiePort::k8105;
    case 8200:
      return CookiePort::k8200;
    case 8201:
      return CookiePort::k8201;
    case 8202:
      return CookiePort::k8202;
    case 8203:
      return CookiePort::k8203;
    case 8204:
      return CookiePort::k8204;
    case 8205:
      return CookiePort::k8205;
    case 8443:
      return CookiePort::k8443;
    case 8444:
      return CookiePort::k8444;
    case 8445:
      return CookiePort::k8445;
    case 8446:
      return CookiePort::k8446;
    case 8447:
      return CookiePort::k8447;
    case 8448:
      return CookiePort::k8448;
    case 8888:
      return CookiePort::k8888;
    case 8889:
      return CookiePort::k8889;
    case 8890:
      return CookiePort::k8890;
    case 8891:
      return CookiePort::k8891;
    case 8892:
      return CookiePort::k8892;
    case 8893:
      return CookiePort::k8893;
    case 9000:
      return CookiePort::k9000;
    case 9001:
      return CookiePort::k9001;
    case 9002:
      return CookiePort::k9002;
    case 9003:
      return CookiePort::k9003;
    case 9004:
      return CookiePort::k9004;
    case 9005:
      return CookiePort::k9005;
    case 9090:
      return CookiePort::k9090;
    case 9091:
      return CookiePort::k9091;
    case 9092:
      return CookiePort::k9092;
    case 9093:
      return CookiePort::k9093;
    case 9094:
      return CookiePort::k9094;
    case 9095:
      return CookiePort::k9095;
    default:
      return CookiePort::kOther;
  }
}

CookieSourceSchemeName GetSchemeNameEnum(const GURL& url) {
  // The most likely schemes are first, to improve performance.
  if (url.SchemeIs(url::kHttpsScheme)) {
    return CookieSourceSchemeName::kHttpsScheme;
  } else if (url.SchemeIs(url::kHttpScheme)) {
    return CookieSourceSchemeName::kHttpScheme;
  } else if (url.SchemeIs(url::kWssScheme)) {
    return CookieSourceSchemeName::kWssScheme;
  } else if (url.SchemeIs(url::kWsScheme)) {
    return CookieSourceSchemeName::kWsScheme;
  } else if (url.SchemeIs("chrome-extension")) {
    return CookieSourceSchemeName::kChromeExtensionScheme;
  } else if (url.SchemeIs(url::kFileScheme)) {
    return CookieSourceSchemeName::kFileScheme;
  }
  // These all aren't marked as cookieable and so are much less likely to
  // occur.
  else if (url.SchemeIs(url::kAboutBlankURL)) {
    return CookieSourceSchemeName::kAboutBlankURL;
  } else if (url.SchemeIs(url::kAboutSrcdocURL)) {
    return CookieSourceSchemeName::kAboutSrcdocURL;
  } else if (url.SchemeIs(url::kAboutBlankPath)) {
    return CookieSourceSchemeName::kAboutBlankPath;
  } else if (url.SchemeIs(url::kAboutSrcdocPath)) {
    return CookieSourceSchemeName::kAboutSrcdocPath;
  } else if (url.SchemeIs(url::kAboutScheme)) {
    return CookieSourceSchemeName::kAboutScheme;
  } else if (url.SchemeIs(url::kBlobScheme)) {
    return CookieSourceSchemeName::kBlobScheme;
  } else if (url.SchemeIs(url::kContentScheme)) {
    return CookieSourceSchemeName::kContentScheme;
  } else if (url.SchemeIs(url::kContentIDScheme)) {
    return CookieSourceSchemeName::kContentIDScheme;
  } else if (url.SchemeIs(url::kDataScheme)) {
    return CookieSourceSchemeName::kDataScheme;
  } else if (url.SchemeIs(url::kFileSystemScheme)) {
    return CookieSourceSchemeName::kFileSystemScheme;
  } else if (url.SchemeIs(url::kFtpScheme)) {
    return CookieSourceSchemeName::kFtpScheme;
  } else if (url.SchemeIs(url::kJavaScriptScheme)) {
    return CookieSourceSchemeName::kJavaScriptScheme;
  } else if (url.SchemeIs(url::kMailToScheme)) {
    return CookieSourceSchemeName::kMailToScheme;
  } else if (url.SchemeIs(url::kTelScheme)) {
    return CookieSourceSchemeName::kTelScheme;
  } else if (url.SchemeIs(url::kUrnScheme)) {
    return CookieSourceSchemeName::kUrnScheme;
  }

  return CookieSourceSchemeName::kOther;
}

const char kEmptyCookiePartitionKey[] = "";

}  // namespace net
