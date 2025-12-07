// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class AboutSigninInternals;
class ProfileIOS;

namespace ios {
// Singleton that owns all AboutSigninInternals and associates them with browser
// states.
class AboutSigninInternalsFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static AboutSigninInternals* GetForProfile(ProfileIOS* profile);
  static AboutSigninInternalsFactory* GetInstance();

 private:
  friend class base::NoDestructor<AboutSigninInternalsFactory>;

  AboutSigninInternalsFactory();
  ~AboutSigninInternalsFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
