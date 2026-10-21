// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"

#import <utility>

#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

namespace enterprise_connectors {

IOSReportingEventRouterFactory::IOSReportingEventRouterFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "IOSReportingEventRouter",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(IOSRealtimeReportingClientFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSReportingEventRouterFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ReportingEventRouter>(
      IOSRealtimeReportingClientFactory::GetForProfile(profile));
}

}  // namespace enterprise_connectors
