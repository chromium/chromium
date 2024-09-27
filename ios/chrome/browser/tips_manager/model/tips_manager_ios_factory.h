// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class TipsManagerIOS;
class KeyedService;
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
namespace web {
class BrowserState;
}  // namespace web

// Singleton that owns all `TipsManagerIOS` objects and associates them
// with Profiles.
class TipsManagerIOSFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the `TipsManagerIOS` instance associated with the given
  // `profile`. If no instance exists, one will be created.
  static TipsManagerIOS* GetForProfile(ProfileIOS* profile);
  // Returns the singleton instance of `TipsManagerIOSFactory`.
  static TipsManagerIOSFactory* GetInstance();

  // Returns the default factory used to build `TipsManagerIOS`. Can be
  // registered with `SetTestingFactory()` to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<TipsManagerIOSFactory>;

  TipsManagerIOSFactory();
  ~TipsManagerIOSFactory() override;

  // `BrowserStateKeyedServiceFactory` implementation.
  // Creates and returns an `TipsManagerIOS` instance for the given
  // `context`.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_FACTORY_H_
