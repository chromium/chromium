// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROTOTYPE_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROTOTYPE_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class GeminiPrototypeOmniboxService;
class ProfileIOS;

// Singleton that owns all GeminiPrototypeOmniboxServiceIOS and associates them
// with a Profile.
class GeminiPrototypeOmniboxServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static GeminiPrototypeOmniboxService* GetForProfile(ProfileIOS* profile);
  static GeminiPrototypeOmniboxServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<GeminiPrototypeOmniboxServiceFactory>;

  GeminiPrototypeOmniboxServiceFactory();
  ~GeminiPrototypeOmniboxServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROTOTYPE_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_FACTORY_H_
