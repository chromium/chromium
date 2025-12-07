// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace TemplateURLPrepopulateData {
class Resolver;
}

namespace ios {

class TemplateURLPrepopulateDataResolverFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static TemplateURLPrepopulateData::Resolver* GetForProfile(
      ProfileIOS* profile);
  static TemplateURLPrepopulateDataResolverFactory* GetInstance();

 private:
  friend class base::NoDestructor<TemplateURLPrepopulateDataResolverFactory>;

  TemplateURLPrepopulateDataResolverFactory();
  ~TemplateURLPrepopulateDataResolverFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_
