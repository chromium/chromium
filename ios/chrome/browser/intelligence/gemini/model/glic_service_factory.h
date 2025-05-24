// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_MODEL_GLIC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_MODEL_GLIC_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class GlicService;
class ProfileIOS;

// Singleton that owns all GlicServices and associates them with a Profile.
class GlicServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static GlicService* GetForProfile(ProfileIOS* profile);
  static GlicServiceFactory* GetInstance();

  // Returns the default factory used to build GlicService. Can be registered
  // with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<GlicServiceFactory>;

  GlicServiceFactory();
  ~GlicServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_MODEL_GLIC_SERVICE_FACTORY_H_
