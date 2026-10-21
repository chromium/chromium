// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRIVATE_AI_MODEL_PRIVATE_AI_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PRIVATE_AI_MODEL_PRIVATE_AI_SERVICE_FACTORY_H_

#include "components/private_ai/private_ai_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class PrivateAiServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<PrivateAiServiceFactory,
                                                private_ai::PrivateAiService> {
 public:
  explicit PrivateAiServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PRIVATE_AI_MODEL_PRIVATE_AI_SERVICE_FACTORY_H_
