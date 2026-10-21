// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_

#import "components/autofill/core/browser/logging/log_router.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all PasswordStores and associates them with
// profile.
class PasswordManagerLogRouterFactory final
    : public TypedProfileKeyedServiceFactoryIOS<PasswordManagerLogRouterFactory,
                                                autofill::LogRouter> {
 public:
  explicit PasswordManagerLogRouterFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios
#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
