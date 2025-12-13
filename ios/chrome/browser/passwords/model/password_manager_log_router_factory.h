// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {
class LogRouter;
}

namespace ios {
// Singleton that owns all PasswordStores and associates them with
// profile.
class PasswordManagerLogRouterFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static autofill::LogRouter* GetForProfile(ProfileIOS* profile);
  static PasswordManagerLogRouterFactory* GetInstance();

 private:
  friend class base::NoDestructor<PasswordManagerLogRouterFactory>;

  PasswordManagerLogRouterFactory();
  ~PasswordManagerLogRouterFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios
#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
