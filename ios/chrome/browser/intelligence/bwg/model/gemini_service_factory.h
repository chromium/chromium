// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_FACTORY_H_

#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all GeminiServices and associates them with a Profile.
class GeminiServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<GeminiServiceFactory,
                                                GeminiService> {
 public:
  explicit GeminiServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_FACTORY_H_
