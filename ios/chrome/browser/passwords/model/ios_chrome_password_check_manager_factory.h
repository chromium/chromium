// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_

#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_refcounted_profile_keyed_service_factory_ios.h"

// Singleton that owns weak pointer to IOSChromePasswordCheckManager.
class IOSChromePasswordCheckManagerFactory
    : public TypedRefcountedProfileKeyedServiceFactoryIOS<
          IOSChromePasswordCheckManagerFactory,
          IOSChromePasswordCheckManager> {
 public:
  IOSChromePasswordCheckManagerFactory(PassKey key);

  // Returns the default factory for tests.
  static TestingFactory GetDefaultFactory();

 private:
  // RefcountedProfileKeyedServiceFactoryIOS implementation.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_
