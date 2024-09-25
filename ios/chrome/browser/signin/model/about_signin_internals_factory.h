// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class AboutSigninInternals;

namespace ios {
// Singleton that owns all AboutSigninInternals and associates them with browser
// states.
class AboutSigninInternalsFactory : public BrowserStateKeyedServiceFactory {
 public:
  static AboutSigninInternals* GetForProfile(ProfileIOS* profile);
  static AboutSigninInternalsFactory* GetInstance();

  AboutSigninInternalsFactory(const AboutSigninInternalsFactory&) = delete;
  AboutSigninInternalsFactory& operator=(const AboutSigninInternalsFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AboutSigninInternalsFactory>;

  AboutSigninInternalsFactory();
  ~AboutSigninInternalsFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
