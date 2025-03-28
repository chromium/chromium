// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"

#import "base/no_destructor.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"
#import "ios/web/public/browser_state.h"

namespace enterprise_connectors {

// static
IOSReportingEventRouterFactory* IOSReportingEventRouterFactory::GetInstance() {
  static base::NoDestructor<IOSReportingEventRouterFactory> instance;
  return instance.get();
}

// static
ReportingEventRouter* IOSReportingEventRouterFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ReportingEventRouter>(
      profile, /*create=*/true);
}

IOSReportingEventRouterFactory::IOSReportingEventRouterFactory()
    : ProfileKeyedServiceFactoryIOS("IOSReportingEventRouter",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(IOSRealtimeReportingClientFactory::GetInstance());
}

IOSReportingEventRouterFactory::~IOSReportingEventRouterFactory() = default;

std::unique_ptr<KeyedService>
IOSReportingEventRouterFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  auto* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<ReportingEventRouter>(
      IOSRealtimeReportingClientFactory::GetForProfile(profile));
}

}  // namespace enterprise_connectors
