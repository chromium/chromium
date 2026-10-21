// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Singleton that owns all `TipsManagerIOS` objects and associates them
// with Profiles.
class TipsManagerIOSFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TipsManagerIOSFactory,
                                                TipsManagerIOS> {
 public:
  explicit TipsManagerIOSFactory(PassKey key);

  // Returns the default factory used to build `TipsManagerIOS`. Can be
  // registered with `SetTestingFactory()` to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // `ProfileKeyedServiceFactoryIOS` implementation.
  // Creates and returns an `TipsManagerIOS` instance for the given
  // `context`.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_FACTORY_H_
