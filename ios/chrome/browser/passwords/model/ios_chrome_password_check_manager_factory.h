// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"

class IOSChromePasswordCheckManager;

// Singleton that owns weak pointer to IOSChromePasswordCheckManager.
class IOSChromePasswordCheckManagerFactory
    : public RefcountedProfileKeyedServiceFactoryIOS {
 public:
  static scoped_refptr<IOSChromePasswordCheckManager> GetForProfile(
      ProfileIOS* profile);
  static IOSChromePasswordCheckManagerFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<IOSChromePasswordCheckManagerFactory>;

  IOSChromePasswordCheckManagerFactory();
  ~IOSChromePasswordCheckManagerFactory() override;

  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_
