// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/cache_util.h"

#include <stddef.h>

#include <string>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/http/http_util.h"
#include "net/http/http_version.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"

namespace blink {

using ::base::Time;
using ::net::HttpVersion;

enum { kHttpOK = 200, kHttpPartialContent = 206 };

uint32_t GetReasonsForUncacheability(const WebURLResponse& response) {
  uint32_t reasons = 0;
  const int code = response.HttpStatusCode();
  const int version = response.HttpVersion();
  const HttpVersion http_version =
      version == WebURLResponse::kHTTPVersion_2_0   ? HttpVersion(2, 0)
      : version == WebURLResponse::kHTTPVersion_1_1 ? HttpVersion(1, 1)
      : version == WebURLResponse::kHTTPVersion_1_0 ? HttpVersion(1, 0)
      : version == WebURLResponse::kHTTPVersion_0_9 ? HttpVersion(0, 9)
                                                    : HttpVersion();
  if (code != kHttpOK && code != kHttpPartialContent)
    reasons |= kNoData;
  if (http_version < HttpVersion(1, 1) && code == kHttpPartialContent)
    reasons |= kPre11PartialResponse;
  if (code == kHttpPartialContent &&
      !net::HttpUtil::HasStrongValidators(
          http_version, response.HttpHeaderField("etag").Utf8(),
          response.HttpHeaderField("Last-Modified").Utf8(),
          response.HttpHeaderField("Date").Utf8())) {
    reasons |= kNoStrongValidatorOnPartialResponse;
  }

  std::string cache_control_header =
      base::ToLowerASCII(response.HttpHeaderField("cache-control").Utf8());

  if (base::Contains(cache_control_header, "no-cache")) {
    reasons |= kNoCache;
  }

  if (base::Contains(cache_control_header, "no-store")) {
    reasons |= kNoStore;
  }

  if (base::Contains(cache_control_header, "must-revalidate")) {
    reasons |= kHasMustRevalidate;
  }

  const base::TimeDelta kMinimumAgeForUsefulness =
      base::Seconds(3600);  // Arbitrary value.

  const char kMaxAgePrefix[] = "max-age=";
  const size_t kMaxAgePrefixLen = std::size(kMaxAgePrefix) - 1;
  if (cache_control_header.substr(0, kMaxAgePrefixLen) == kMaxAgePrefix) {
    int64_t max_age_seconds;
    base::StringToInt64(
        base::MakeStringPiece(cache_control_header.begin() + kMaxAgePrefixLen,
                              cache_control_header.end()),
        &max_age_seconds);
    if (base::Seconds(max_age_seconds) < kMinimumAgeForUsefulness) {
      reasons |= kShortMaxAge;
    }
  }

  Time date;
  Time expires;
  if (Time::FromString(response.HttpHeaderField("Date").Utf8().data(), &date) &&
      Time::FromString(response.HttpHeaderField("Expires").Utf8().data(),
                       &expires) &&
      date > Time() && expires > Time() &&
      (expires - date) < kMinimumAgeForUsefulness) {
    reasons |= kExpiresTooSoon;
  }

  return reasons;
}

base::TimeDelta GetCacheValidUntil(const WebURLResponse& response) {
  std::string cache_control_header =
      base::ToLowerASCII(response.HttpHeaderField("cache-control").Utf8());

  if (base::Contains(cache_control_header, "no-cache") ||
      base::Contains(cache_control_header, "must-revalidate")) {
    return base::TimeDelta();
  }

  // Max cache timeout ~= 1 month.
  base::TimeDelta ret = base::Days(30);

  const char kMaxAgePrefix[] = "max-age=";
  const size_t kMaxAgePrefixLen = std::size(kMaxAgePrefix) - 1;
  if (cache_control_header.substr(0, kMaxAgePrefixLen) == kMaxAgePrefix) {
    int64_t max_age_seconds;
    base::StringToInt64(
        base::MakeStringPiece(cache_control_header.begin() + kMaxAgePrefixLen,
                              cache_control_header.end()),
        &max_age_seconds);

    ret = std::min(ret, base::Seconds(max_age_seconds));
  } else {
    // Note that |date| may be smaller than |expires|, which means we'll
    // return a timetick some time in the past.
    Time date;
    Time expires;
    if (Time::FromString(response.HttpHeaderField("Date").Utf8().data(),
                         &date) &&
        Time::FromString(response.HttpHeaderField("Expires").Utf8().data(),
                         &expires) &&
        date > Time() && expires > Time()) {
      ret = std::min(ret, expires - date);
    }
  }

  return ret;
}

}  // namespace blink
