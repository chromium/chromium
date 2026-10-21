// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_FACTORY_H_

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace enterprise_connectors {

class BackgroundCloudScannerManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          BackgroundCloudScannerManagerFactory,
          BackgroundCloudScannerManager> {
 public:
  explicit BackgroundCloudScannerManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_FACTORY_H_
