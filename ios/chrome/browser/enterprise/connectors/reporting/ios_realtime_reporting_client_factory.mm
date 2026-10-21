// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"

#import <utility>

#import "components/keyed_service/core/keyed_service_base_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace enterprise_connectors {

namespace {

std::unique_ptr<KeyedService> BuildRealtimeReportingClient(
    ProfileIOS* profile) {
  return std::make_unique<IOSRealtimeReportingClient>(profile);
}

}  // namespace

// static
IOSRealtimeReportingClientFactory::TestingFactory
IOSRealtimeReportingClientFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildRealtimeReportingClient);
}

IOSRealtimeReportingClientFactory::IOSRealtimeReportingClientFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "IOSRealtimeReportingClient",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ConnectorsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSRealtimeReportingClientFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildRealtimeReportingClient(profile);
}

}  // namespace enterprise_connectors
