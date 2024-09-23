// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class TemplateURLFetcher;

namespace ios {
// Singleton that owns all TemplateURLFetchers and associates them with
// Profile.
class TemplateURLFetcherFactory : public BrowserStateKeyedServiceFactory {
 public:
  static TemplateURLFetcher* GetForProfile(ProfileIOS* profile);
  static TemplateURLFetcherFactory* GetInstance();

  TemplateURLFetcherFactory(const TemplateURLFetcherFactory&) = delete;
  TemplateURLFetcherFactory& operator=(const TemplateURLFetcherFactory&) =
      delete;

 private:
  friend class base::NoDestructor<TemplateURLFetcherFactory>;

  TemplateURLFetcherFactory();
  ~TemplateURLFetcherFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_
