// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_

#import "components/search_engines/template_url_prepopulate_data_resolver.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

class TemplateURLPrepopulateDataResolverFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          TemplateURLPrepopulateDataResolverFactory,
          TemplateURLPrepopulateData::Resolver> {
 public:
  explicit TemplateURLPrepopulateDataResolverFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_
