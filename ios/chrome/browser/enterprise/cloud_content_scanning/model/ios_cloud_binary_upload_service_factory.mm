// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service_factory.h"

#import <utility>

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_connectors {

IOSCloudBinaryUploadServiceFactory::IOSCloudBinaryUploadServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "IOSCloudBinaryUploadService",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(policy::BrowserManagementServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSCloudBinaryUploadServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<CloudBinaryUploadServiceBase>(
      profile->GetSharedURLLoaderFactory(),
      std::make_unique<IOSCloudBinaryUploadService>(profile));
}

}  // namespace enterprise_connectors
