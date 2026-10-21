// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_FACTORY_H_

#import <memory>

#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all TemplateURLServices and associates them with
// Profile.
class TemplateURLServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TemplateURLServiceFactory,
                                                TemplateURLService> {
 public:
  explicit TemplateURLServiceFactory(PassKey key);

  // Returns the default factory used to build TemplateURLServices. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_FACTORY_H_
