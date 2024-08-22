// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class SigninBrowserStateInfoUpdater;

// TODO(crbug.com/361040573): Rename this class to
// SigninProfileInfoUpdaterFactory.
class SigninBrowserStateInfoUpdaterFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns nullptr if this browser state cannot have a
  // SigninBrowserStateInfoUpdater (for example, if it is incognito).
  static SigninBrowserStateInfoUpdater* GetForBrowserState(
      ChromeBrowserState* chrome_browser_state);

  // Returns an instance of the factory singleton.
  static SigninBrowserStateInfoUpdaterFactory* GetInstance();

  SigninBrowserStateInfoUpdaterFactory(
      const SigninBrowserStateInfoUpdaterFactory&) = delete;
  SigninBrowserStateInfoUpdaterFactory& operator=(
      const SigninBrowserStateInfoUpdaterFactory&) = delete;

 private:
  friend class base::NoDestructor<SigninBrowserStateInfoUpdaterFactory>;

  SigninBrowserStateInfoUpdaterFactory();
  ~SigninBrowserStateInfoUpdaterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* state) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_
