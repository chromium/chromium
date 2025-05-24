// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/reporting_util.h"

#import <string.h>

#import "base/check.h"
#import "base/feature_list.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/enterprise/connectors/features.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/web_state.h"
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

void ReportEnterpriseUrlFilteringEvent(UrlFilteringEventType event_type,
                                       const GURL& page_url,
                                       web::WebState* web_state) {
  if (!base::FeatureList::IsEnabled(kEnterpriseRealtimeEventReportingOnIOS)) {
    return;
  }
  if (!base::FeatureList::IsEnabled(kIOSEnterpriseRealtimeUrlFiltering)) {
    return;
  }

  CHECK(web_state);
  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(web_state);
  // There is no container when opening the interstitial directly
  // through the chrome://interstitials WebUI page. No-op as there was no real
  // Url Filtering event.
  if (!container) {
    return;
  }

  const security_interstitials::UnsafeResource* resource =
      container->GetMainFrameUnsafeResource();
  CHECK(resource);

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  CHECK(profile);

  ReportingEventRouter* event_router =
      IOSReportingEventRouterFactory::GetForProfile(profile);
  CHECK(event_router);
  // ReferrerChain is not supported on ios for now.
  event_router->OnUrlFilteringInterstitial(
      page_url, GetEventTypeString(event_type), resource->rt_lookup_response,
      /*referrer_chain=*/{});
}

}  // namespace enterprise_connectors
