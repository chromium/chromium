// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace search_engines {
class SearchEngineChoiceService;
}

namespace ios {

class SearchEngineChoiceServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static search_engines::SearchEngineChoiceService* GetForProfile(
      ProfileIOS* profile);
  static SearchEngineChoiceServiceFactory* GetInstance();

  SearchEngineChoiceServiceFactory(const SearchEngineChoiceServiceFactory&) =
      delete;
  SearchEngineChoiceServiceFactory& operator=(
      const SearchEngineChoiceServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<SearchEngineChoiceServiceFactory>;

  SearchEngineChoiceServiceFactory();
  ~SearchEngineChoiceServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_
