// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_

#include "components/search_engines/template_url_fetcher.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all TemplateURLFetchers and associates them with
// Profile.
class TemplateURLFetcherFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TemplateURLFetcherFactory,
                                                TemplateURLFetcher> {
 public:
  explicit TemplateURLFetcherFactory(PassKey key);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_FETCHER_FACTORY_H_
