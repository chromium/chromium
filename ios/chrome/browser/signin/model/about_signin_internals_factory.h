// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_

#import "components/signin/core/browser/about_signin_internals.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all AboutSigninInternals and associates them with browser
// states.
class AboutSigninInternalsFactory final
    : public TypedProfileKeyedServiceFactoryIOS<AboutSigninInternalsFactory,
                                                AboutSigninInternals> {
 public:
  explicit AboutSigninInternalsFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
