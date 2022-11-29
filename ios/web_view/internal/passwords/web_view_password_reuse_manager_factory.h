// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_REUSE_MANAGER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_REUSE_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace password_manager {
class PasswordReuseManager;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all PasswordReuseMnagers and associates them with
// WebViewBrowserState.
class WebViewPasswordReuseManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static password_manager::PasswordReuseManager* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewPasswordReuseManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewPasswordReuseManagerFactory>;

  WebViewPasswordReuseManagerFactory();
  ~WebViewPasswordReuseManagerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_REUSE_MANAGER_FACTORY_H_
