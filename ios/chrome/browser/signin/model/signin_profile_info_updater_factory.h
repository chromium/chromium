// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/signin/model/signin_profile_info_updater.h"

// Owns all SigninProfileInfoUpdater instances and associates them with
// profiles.
class SigninProfileInfoUpdaterFactory final
    : public TypedProfileKeyedServiceFactoryIOS<SigninProfileInfoUpdaterFactory,
                                                SigninProfileInfoUpdater> {
 public:
  explicit SigninProfileInfoUpdaterFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_
