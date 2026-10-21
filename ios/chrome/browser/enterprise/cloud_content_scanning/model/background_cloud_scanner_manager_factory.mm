// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager_factory.h"

#import <utility>

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_connectors {

BackgroundCloudScannerManagerFactory::BackgroundCloudScannerManagerFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "BackgroundCloudScannerManager",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(IOSCloudBinaryUploadServiceFactory::GetInstance());
  DependsOn(ConnectorsServiceFactory::GetInstance());
  DependsOn(IOSReportingEventRouterFactory::GetInstance());
}

std::unique_ptr<KeyedService>
BackgroundCloudScannerManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<BackgroundCloudScannerManager>(
      ConnectorsServiceFactory::GetForProfile(profile),
      IOSReportingEventRouterFactory::GetForProfile(profile),
      IOSCloudBinaryUploadServiceFactory::GetForProfile(profile));
}

}  // namespace enterprise_connectors
