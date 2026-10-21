// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory for `TTCKeyedService`.
class TTCKeyedServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TTCKeyedServiceFactory,
                                                TTCKeyedService> {
 public:
  explicit TTCKeyedServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_FACTORY_H_
