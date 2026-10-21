// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_

#import "base/memory/scoped_refptr.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "ios/chrome/browser/shared/model/profile/typed_refcounted_profile_keyed_service_factory_ios.h"

// Singleton that owns all Gaia-account-scoped PasswordStores and associates
// them with ProfileIOS.
class IOSChromeAccountPasswordStoreFactory final
    : public TypedRefcountedProfileKeyedServiceFactoryIOS<
          IOSChromeAccountPasswordStoreFactory,
          password_manager::PasswordStoreInterface,
          RejectImplicitAccessIfIncognito> {
 public:
  explicit IOSChromeAccountPasswordStoreFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_
