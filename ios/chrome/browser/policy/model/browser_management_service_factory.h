// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_MANAGEMENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_MANAGEMENT_SERVICE_FACTORY_H_

#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace policy {

class ManagementService;

class BrowserManagementServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<BrowserManagementServiceFactory,
                                                BrowserManagementService> {
 public:
  explicit BrowserManagementServiceFactory(PassKey key);
  static ManagementService* GetForPlatform();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_MANAGEMENT_SERVICE_FACTORY_H_
