// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GCM_MODEL_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GCM_MODEL_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_

#import <memory>
#import <string>

#import "components/gcm_driver/gcm_profile_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all GCMProfileService and associates them with
// profiles.
class IOSChromeGCMProfileServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeGCMProfileServiceFactory,
          gcm::GCMProfileService> {
 public:
  explicit IOSChromeGCMProfileServiceFactory(PassKey key);

  // Returns a string like "com.chrome.ios" that should be used as the GCM
  // category when an app_id is sent as a subtype instead of as a category. This
  // string must never change during the lifetime of a Chrome install, since
  // e.g. to unregister an Instance ID token the same category must be passed to
  // GCM as was originally passed when registering it.
  static std::string GetProductCategoryForSubtypes();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_GCM_MODEL_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_
