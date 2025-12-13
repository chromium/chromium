// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GCM_MODEL_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GCM_MODEL_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_

#import <memory>
#import <string>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace gcm {
class GCMProfileService;
}

// Singleton that owns all GCMProfileService and associates them with
// profiles.
class IOSChromeGCMProfileServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static gcm::GCMProfileService* GetForProfile(ProfileIOS* profile);
  static IOSChromeGCMProfileServiceFactory* GetInstance();

  // Returns a string like "com.chrome.ios" that should be used as the GCM
  // category when an app_id is sent as a subtype instead of as a category. This
  // string must never change during the lifetime of a Chrome install, since
  // e.g. to unregister an Instance ID token the same category must be passed to
  // GCM as was originally passed when registering it.
  static std::string GetProductCategoryForSubtypes();

 private:
  friend class base::NoDestructor<IOSChromeGCMProfileServiceFactory>;

  IOSChromeGCMProfileServiceFactory();
  ~IOSChromeGCMProfileServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_GCM_MODEL_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_
