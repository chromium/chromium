// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_CLIENT_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_CLIENT_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class SigninClient;

// Singleton that owns all SigninClients and associates them with
// ChromeBrowserState.
class SigninClientFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SigninClient* GetForBrowserState(ChromeBrowserState* browser_state);
  static SigninClientFactory* GetInstance();

  SigninClientFactory(const SigninClientFactory&) = delete;
  SigninClientFactory& operator=(const SigninClientFactory&) = delete;

 private:
  friend class base::NoDestructor<SigninClientFactory>;

  SigninClientFactory();
  ~SigninClientFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_CLIENT_FACTORY_H_
