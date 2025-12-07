// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ImpressionLimitService;

// Owns all ImpressionLimitService instances and associates them to profiles.
class ImpressionLimitServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ImpressionLimitServiceFactory* GetInstance();

  static ImpressionLimitService* GetForProfile(ProfileIOS* profile);
  static ImpressionLimitService* GetForProfileIfExists(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<ImpressionLimitServiceFactory>;

  ImpressionLimitServiceFactory();
  ~ImpressionLimitServiceFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_FACTORY_H_
