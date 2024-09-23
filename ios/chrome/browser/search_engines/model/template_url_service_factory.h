// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class TemplateURLService;

namespace ios {
// Singleton that owns all TemplateURLServices and associates them with
// Profile.
class TemplateURLServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static TemplateURLService* GetForBrowserState(ProfileIOS* profile);

  static TemplateURLService* GetForProfile(ProfileIOS* profile);
  static TemplateURLServiceFactory* GetInstance();

  // Returns the default factory used to build TemplateURLServices. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  TemplateURLServiceFactory(const TemplateURLServiceFactory&) = delete;
  TemplateURLServiceFactory& operator=(const TemplateURLServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<TemplateURLServiceFactory>;

  TemplateURLServiceFactory();
  ~TemplateURLServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_FACTORY_H_
