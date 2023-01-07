// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_STORE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_STORE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

enum class ServiceAccessType;

namespace password_manager {
class PasswordStoreInterface;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all PasswordStores and associates them with
// WebViewBrowserState.
class WebViewPasswordStoreFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface>
  GetForBrowserState(WebViewBrowserState* browser_state,
                     ServiceAccessType access_type);

  static WebViewPasswordStoreFactory* GetInstance();

  WebViewPasswordStoreFactory(const WebViewPasswordStoreFactory&) = delete;
  WebViewPasswordStoreFactory& operator=(const WebViewPasswordStoreFactory&) =
      delete;

 private:
  friend class base::NoDestructor<WebViewPasswordStoreFactory>;

  WebViewPasswordStoreFactory();
  ~WebViewPasswordStoreFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_STORE_FACTORY_H_
