// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/reporting_util.h"

#import <string.h>

#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "url/gurl.h"

namespace {

using enterprise_connectors::UrlFilteringEventType;

// Converts the UrlFilteringEventType enum to the string expected by the
// ReportingEventRouter.
std::string GetEventTypeString(UrlFilteringEventType event_type) {
  switch (event_type) {
    case UrlFilteringEventType::kBlockedSeen:
      return "ENTERPRISE_BLOCKED_SEEN";
    case UrlFilteringEventType::kWarnedSeen:
      return "ENTERPRISE_WARNED_SEEN";
    case UrlFilteringEventType::kBypassed:
      return "ENTERPRISE_WARNED_BYPASS";
  }
}

}  // namespace

namespace enterprise_connectors {

void ReportEnterpriseUrlFilteringEvent(
    UrlFilteringEventType event_type,
    const GURL& page_url,
    const safe_browsing::RTLookupResponse& rt_lookup_response,
    ReportingEventRouter* event_router) {
  CHECK(event_router);

  // ReferrerChain is not supported on ios for now.
  event_router->OnUrlFilteringInterstitial(
      page_url, GetEventTypeString(event_type), rt_lookup_response,
      /*referrer_chain=*/{});
}

}  // namespace enterprise_connectors
