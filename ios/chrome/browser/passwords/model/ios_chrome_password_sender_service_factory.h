// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace password_manager {
class PasswordSenderService;
}

// Creates instances of PasswordSenderService per profile.
class IOSChromePasswordSenderServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static password_manager::PasswordSenderService* GetForBrowserState(
      ProfileIOS* profile);

  static password_manager::PasswordSenderService* GetForProfile(
      ProfileIOS* profile);
  static IOSChromePasswordSenderServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromePasswordSenderServiceFactory>;

  IOSChromePasswordSenderServiceFactory();
  ~IOSChromePasswordSenderServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_
