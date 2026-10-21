// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AI_MODE_BUTTON_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AI_MODE_BUTTON_SERVICE_FACTORY_H_

#import "components/search_engines/ai_mode_button_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all AiModeButtonServices and associates them with
// ProfileIOS.
class IOSChromeAiModeButtonServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeAiModeButtonServiceFactory,
          AiModeButtonService> {
 public:
  explicit IOSChromeAiModeButtonServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AI_MODE_BUTTON_SERVICE_FACTORY_H_
