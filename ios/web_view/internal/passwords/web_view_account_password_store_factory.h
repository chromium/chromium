// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_ACCOUNT_PASSWORD_STORE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_ACCOUNT_PASSWORD_STORE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// Singleton that owns all Gaia-account-scoped PasswordStores and associates
// them with WebViewBrowserStates.
class WebViewAccountPasswordStoreFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface>
  GetForBrowserState(WebViewBrowserState* browser_state,
                     ServiceAccessType access_type);

  static WebViewAccountPasswordStoreFactory* GetInstance();

  WebViewAccountPasswordStoreFactory(
      const WebViewAccountPasswordStoreFactory&) = delete;
  WebViewAccountPasswordStoreFactory& operator=(
      const WebViewAccountPasswordStoreFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewAccountPasswordStoreFactory>;

  WebViewAccountPasswordStoreFactory();
  ~WebViewAccountPasswordStoreFactory() override;

  // Overrides of methods in BrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_ACCOUNT_PASSWORD_STORE_FACTORY_H_
