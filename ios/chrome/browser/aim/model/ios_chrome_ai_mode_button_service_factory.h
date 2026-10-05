// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AI_MODE_BUTTON_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AI_MODE_BUTTON_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class AiModeButtonService;
class ProfileIOS;

// Singleton that owns all AiModeButtonServices and associates them with
// ProfileIOS.
class IOSChromeAiModeButtonServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static AiModeButtonService* GetForProfile(ProfileIOS* profile);
  static IOSChromeAiModeButtonServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeAiModeButtonServiceFactory>;

  IOSChromeAiModeButtonServiceFactory();
  ~IOSChromeAiModeButtonServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AI_MODE_BUTTON_SERVICE_FACTORY_H_
