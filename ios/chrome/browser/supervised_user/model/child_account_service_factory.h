// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_CHILD_ACCOUNT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_CHILD_ACCOUNT_SERVICE_FACTORY_H_

#import "components/supervised_user/core/browser/child_account_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns ChildAccountService objects and associates
// them with Profiles.
class ChildAccountServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ChildAccountServiceFactory,
          supervised_user::ChildAccountService> {
 public:
  explicit ChildAccountServiceFactory(PassKey key);
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_CHILD_ACCOUNT_SERVICE_FACTORY_H_
