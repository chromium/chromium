// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class GeminiCapabilitiesManager;
class ProfileIOS;

// Singleton that owns all GeminiCapabilitiesManagers and associates them with a
// Profile.
class GeminiCapabilitiesManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static GeminiCapabilitiesManager* GetForProfile(ProfileIOS* profile);
  static GeminiCapabilitiesManagerFactory* GetInstance();

  // Returns the default factory used to build GeminiCapabilitiesManager.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<GeminiCapabilitiesManagerFactory>;

  GeminiCapabilitiesManagerFactory();
  ~GeminiCapabilitiesManagerFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_FACTORY_H_
