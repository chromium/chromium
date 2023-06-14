// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_AFFILIATIONS_PREFETCHER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_AFFILIATIONS_PREFETCHER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace password_manager {
class AffiliationsPrefetcher;
}  // namespace password_manager

// Creates instances of AffiliationsPrefetcher per BrowserState.
class WebViewAffiliationsPrefetcherFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static WebViewAffiliationsPrefetcherFactory* GetInstance();
  static password_manager::AffiliationsPrefetcher* GetForBrowserState(
      web::BrowserState* browser_state);

 private:
  friend class base::NoDestructor<WebViewAffiliationsPrefetcherFactory>;

  WebViewAffiliationsPrefetcherFactory();
  ~WebViewAffiliationsPrefetcherFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_AFFILIATIONS_PREFETCHER_FACTORY_H_
