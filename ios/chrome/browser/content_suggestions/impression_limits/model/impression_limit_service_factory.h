// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_FACTORY_H_

#include <memory>

#include "ios/chrome/browser/content_suggestions/impression_limits/model/impression_limit_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Owns all ImpressionLimitService instances and associates them to profiles.
class ImpressionLimitServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ImpressionLimitServiceFactory,
                                                ImpressionLimitService> {
 public:
  explicit ImpressionLimitServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_FACTORY_H_
