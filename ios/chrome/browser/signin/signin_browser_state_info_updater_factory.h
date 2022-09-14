// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class SigninBrowserStateInfoUpdater;

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
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_FACTORY_H_
