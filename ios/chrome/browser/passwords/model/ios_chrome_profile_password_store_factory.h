// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_

#import "base/memory/scoped_refptr.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "ios/chrome/browser/shared/model/profile/typed_refcounted_profile_keyed_service_factory_ios.h"

namespace password_manager {
class PasswordStoreInterface;
}

// Singleton that owns all PasswordStores and associates them with
// ProfileIOS.
class IOSChromeProfilePasswordStoreFactory
    : public TypedRefcountedProfileKeyedServiceFactoryIOS<
          IOSChromeProfilePasswordStoreFactory,
          password_manager::PasswordStoreInterface,
          UseServiceAccess> {
 public:
  IOSChromeProfilePasswordStoreFactory(PassKey key);

 private:
  // RefcountedProfileKeyedServiceFactoryIOS implementation.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_
