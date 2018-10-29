// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class TemplateURLFetcher;

namespace ios {

class ChromeBrowserState;

// Singleton that owns all TemplateURLFetchers and associates them with
// ios::ChromeBrowserState.
class TemplateURLFetcherFactory : public BrowserStateKeyedServiceFactory {
 public:
  static TemplateURLFetcher* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static TemplateURLFetcherFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<TemplateURLFetcherFactory>;

  TemplateURLFetcherFactory();
  ~TemplateURLFetcherFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_FACTORY_H_
