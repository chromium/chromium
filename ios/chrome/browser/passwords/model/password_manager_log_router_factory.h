// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace autofill {
class LogRouter;
}

namespace ios {
// Singleton that owns all PasswordStores and associates them with
// profile.
class PasswordManagerLogRouterFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static autofill::LogRouter* GetForBrowserState(ProfileIOS* profile);

  static autofill::LogRouter* GetForProfile(ProfileIOS* profile);
  static PasswordManagerLogRouterFactory* GetInstance();

  PasswordManagerLogRouterFactory(const PasswordManagerLogRouterFactory&) =
      delete;
  PasswordManagerLogRouterFactory& operator=(
      const PasswordManagerLogRouterFactory&) = delete;

 private:
  friend class base::NoDestructor<PasswordManagerLogRouterFactory>;

  PasswordManagerLogRouterFactory();
  ~PasswordManagerLogRouterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios
#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
