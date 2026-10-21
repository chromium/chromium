// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_

#import "components/password_manager/core/browser/password_reuse_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Creates instances of PasswordReuseManager per profile.
class IOSChromePasswordReuseManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromePasswordReuseManagerFactory,
          password_manager::PasswordReuseManager> {
 public:
  explicit IOSChromePasswordReuseManagerFactory(PassKey key);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_
