// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_IDENTITY_MANAGER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_IDENTITY_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace signin {
class IdentityManager;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all IdentityManager instances and associates them with
// BrowserStates.
class WebViewIdentityManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static signin::IdentityManager* GetForBrowserState(
      WebViewBrowserState* browser_state);

  // Returns an instance of the WebViewIdentityManagerFactory singleton.
  static WebViewIdentityManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewIdentityManagerFactory>;

  WebViewIdentityManagerFactory();
  ~WebViewIdentityManagerFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewIdentityManagerFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_IDENTITY_MANAGER_FACTORY_H_
