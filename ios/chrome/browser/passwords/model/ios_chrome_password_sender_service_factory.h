// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace password_manager {
class PasswordSenderService;
}

// Creates instances of PasswordSenderService per profile.
class IOSChromePasswordSenderServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the factory instance.
  static IOSChromePasswordSenderServiceFactory* GetInstance();

  // Returns the IOSChromePasswordSenderServiceFactory associated with
  // `profile`.
  static password_manager::PasswordSenderService* GetForProfile(
      ProfileIOS* profile);

 private:
  friend class base::NoDestructor<IOSChromePasswordSenderServiceFactory>;

  IOSChromePasswordSenderServiceFactory();
  ~IOSChromePasswordSenderServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_
