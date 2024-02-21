// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AFFILIATIONS_WEB_VIEW_AFFILIATION_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_AFFILIATIONS_WEB_VIEW_AFFILIATION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace affiliations {
class AffiliationService;
}

namespace ios_web_view {

class WebViewAffiliationServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static WebViewAffiliationServiceFactory* GetInstance();
  static affiliations::AffiliationService* GetForBrowserState(
      web::BrowserState* browser_state);

 private:
  friend class base::NoDestructor<WebViewAffiliationServiceFactory>;

  WebViewAffiliationServiceFactory();
  ~WebViewAffiliationServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};
}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AFFILIATIONS_WEB_VIEW_AFFILIATION_SERVICE_FACTORY_H_
