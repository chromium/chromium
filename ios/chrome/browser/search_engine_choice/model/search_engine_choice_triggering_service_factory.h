// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace ios {

class SearchEngineChoiceTriggeringService;

// Singleton that owns all SearchEngineChoiceTriggeringService and associates
// them with ChromeBrowserState.
class SearchEngineChoiceTriggeringServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static SearchEngineChoiceTriggeringServiceFactory* GetInstance();
  static SearchEngineChoiceTriggeringService* GetForProfile(
      ProfileIOS* profile);

 private:
  friend class base::NoDestructor<SearchEngineChoiceTriggeringServiceFactory>;

  SearchEngineChoiceTriggeringServiceFactory();
  ~SearchEngineChoiceTriggeringServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios
#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_FACTORY_H_
