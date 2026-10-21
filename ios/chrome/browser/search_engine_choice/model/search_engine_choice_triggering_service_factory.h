// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_FACTORY_H_

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_triggering_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

// Singleton that owns all SearchEngineChoiceTriggeringService and associates
// them with ChromeBrowserState.
class SearchEngineChoiceTriggeringServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SearchEngineChoiceTriggeringServiceFactory,
          SearchEngineChoiceTriggeringService> {
 public:
  explicit SearchEngineChoiceTriggeringServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios
#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_FACTORY_H_
