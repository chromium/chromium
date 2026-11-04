// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_utils.h"

#include <algorithm>
#include <string_view>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

base::cstring_view BtmCookieModeToString(BtmCookieMode mode) {
  switch (mode) {
    case BtmCookieMode::kBlock3PC:
      return "Block3PC";
    case BtmCookieMode::kOffTheRecord_Block3PC:
      return "OffTheRecord_Block3PC";
  }
}

base::cstring_view BtmRedirectTypeToString(BtmRedirectType type) {
  switch (type) {
    case BtmRedirectType::kClient:
      return "Client";
    case BtmRedirectType::kServer:
      return "Server";
  }
}

base::cstring_view BtmDataAccessTypeToString(BtmDataAccessType type) {
  switch (type) {
    case BtmDataAccessType::kUnknown:
      return "Unknown";
    case BtmDataAccessType::kNone:
      return "None";
    case BtmDataAccessType::kRead:
      return "Read";
    case BtmDataAccessType::kWrite:
      return "Write";
    case BtmDataAccessType::kReadWrite:
      return "ReadWrite";
  }
}

base::FilePath GetBtmFilePath(BrowserContext* context) {
  return context->GetPath().Append(kBtmFilename);
}

bool UpdateTimestampRange(TimestampRange& range, base::Time time) {
  if (!range.has_value()) {
    range = {time, time};
    return true;
  }

  if (time < range->first) {
    range->first = time;
    return true;
  }

  if (time > range->second) {
    range->second = time;
    return true;
  }

  return false;
}

bool IsNullOrWithin(const TimestampRange& inner, const TimestampRange& outer) {
  if (!inner.has_value()) {
    return true;
  }

  if (!outer.has_value()) {
    return false;
  }

  return outer->first <= inner->first && inner->second <= outer->second;
}

std::ostream& operator<<(std::ostream& os, TimestampRange range) {
  if (!range.has_value()) {
    return os << "[NULL, NULL]";
  }
  return os << "[" << range->first << ", " << range->second << "]";
}

// BtmDataAccessType:
std::ostream& operator<<(std::ostream& os, BtmDataAccessType access_type) {
  return os << BtmDataAccessTypeToString(access_type);
}

// BtmCookieMode:
BtmCookieMode GetBtmCookieMode(bool is_otr) {
  return is_otr ? BtmCookieMode::kOffTheRecord_Block3PC
                : BtmCookieMode::kBlock3PC;
}

std::string_view GetHistogramSuffix(BtmCookieMode mode) {
  // Any changes here need to be reflected in DIPSCookieMode in
  // tools/metrics/histograms/metadata/others/histograms.xml
  switch (mode) {
    case BtmCookieMode::kBlock3PC:
      return ".Block3PC";
    case BtmCookieMode::kOffTheRecord_Block3PC:
      return ".OffTheRecord_Block3PC";
  }
  DCHECK(false) << "Invalid BtmCookieMode";
  return std::string_view();
}

std::ostream& operator<<(std::ostream& os, BtmCookieMode mode) {
  return os << BtmCookieModeToString(mode);
}

// BtmRedirectType:
std::string_view GetHistogramPiece(BtmRedirectType type) {
  // Any changes here need to be reflected in
  // tools/metrics/histograms/metadata/privacy/histograms.xml
  switch (type) {
    case BtmRedirectType::kClient:
      return "Client";
    case BtmRedirectType::kServer:
      return "Server";
  }
  DCHECK(false) << "Invalid BtmRedirectType";
  return std::string_view();
}

std::ostream& operator<<(std::ostream& os, BtmRedirectType type) {
  return os << BtmRedirectTypeToString(type);
}

int64_t BucketizeBtmBounceDelay(base::TimeDelta delta) {
  return std::clamp(delta.InSeconds(), INT64_C(0), INT64_C(10));
}

std::string GetSiteForBtm(const GURL& url) {
  const auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? url.GetHost() : domain;
}

std::string GetSiteForBtm(const url::Origin& origin) {
  const auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? origin.host() : domain;
}

bool UpdateTimestamp(std::optional<base::Time>& last_time, base::Time now) {
  if (!last_time.has_value() ||
      (now - last_time.value()) >= kBtmTimestampUpdateInterval) {
    last_time = now;
    return true;
  }

  return false;
}

bool HasCHIPS(const net::CookieAccessResultList& cookie_access_result_list) {
  for (const auto& cookie_with_access_result : cookie_access_result_list) {
    if (cookie_with_access_result.cookie.IsPartitioned()) {
      return true;
    }
  }
  return false;
}

}  // namespace content
