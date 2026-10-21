// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_

#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

class SearchEngineChoiceServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SearchEngineChoiceServiceFactory,
          search_engines::SearchEngineChoiceService> {
 public:
  explicit SearchEngineChoiceServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_CHOICE_SERVICE_FACTORY_H_
