// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_FACTORY_H_

#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// KeyedServiceFactory for PersonalContextFirstRunService on iOS.
class IOSPersonalContextFirstRunServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSPersonalContextFirstRunServiceFactory,
          personal_context::PersonalContextFirstRunService> {
 public:
  explicit IOSPersonalContextFirstRunServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_FACTORY_H_
