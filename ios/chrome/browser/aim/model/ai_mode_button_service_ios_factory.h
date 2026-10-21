// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_FACTORY_H_

#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all AIModeButtonServiceIOS instances and associates them
// with ProfileIOS.
class AIModeButtonServiceIOSFactory final
    : public TypedProfileKeyedServiceFactoryIOS<AIModeButtonServiceIOSFactory,
                                                AIModeButtonServiceIOS> {
 public:
  explicit AIModeButtonServiceIOSFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_FACTORY_H_
