// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_FACTORY_H_

#import <memory>

#import "components/autofill/core/browser/data_manager/autofill_ai/personal_context_entity_metadata_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all PersonalContextEntityMetadataManagers and associates
// them with ProfileIOS.
class IOSAutofillPersonalContextEntityMetadataManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSAutofillPersonalContextEntityMetadataManagerFactory,
          autofill::PersonalContextEntityMetadataManager> {
 public:
  explicit IOSAutofillPersonalContextEntityMetadataManagerFactory(PassKey key);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS& profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_FACTORY_H_
