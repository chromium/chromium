// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"

#import "components/keyed_service/core/keyed_service_base_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace enterprise_connectors {

namespace {

std::unique_ptr<KeyedService> BuildRealtimeReportingClient(
    web::BrowserState* browser_state) {
  auto* profile = ProfileIOS::FromBrowserState(browser_state);
  DCHECK(profile);
  return std::make_unique<IOSRealtimeReportingClient>(profile);
}

}  // namespace

// static
IOSRealtimeReportingClientFactory*
IOSRealtimeReportingClientFactory::GetInstance() {
  static base::NoDestructor<IOSRealtimeReportingClientFactory> instance;
  return instance.get();
}

// static
IOSRealtimeReportingClient* IOSRealtimeReportingClientFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<IOSRealtimeReportingClient>(
      profile, /*create=*/true);
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
IOSRealtimeReportingClientFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildRealtimeReportingClient);
}

IOSRealtimeReportingClientFactory::IOSRealtimeReportingClientFactory()
    : ProfileKeyedServiceFactoryIOS("IOSRealtimeReportingClient",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ConnectorsServiceFactory::GetInstance());
}

IOSRealtimeReportingClientFactory::~IOSRealtimeReportingClientFactory() =
    default;

std::unique_ptr<KeyedService>
IOSRealtimeReportingClientFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  auto* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<IOSRealtimeReportingClient>(profile);
}

}  // namespace enterprise_connectors
