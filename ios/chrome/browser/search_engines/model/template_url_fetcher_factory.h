// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class TemplateURLFetcher;

namespace ios {
// Singleton that owns all TemplateURLFetchers and associates them with
// Profile.
class TemplateURLFetcherFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static TemplateURLFetcher* GetForProfile(ProfileIOS* profile);
  static TemplateURLFetcherFactory* GetInstance();

 private:
  friend class base::NoDestructor<TemplateURLFetcherFactory>;

  TemplateURLFetcherFactory();
  ~TemplateURLFetcherFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_
