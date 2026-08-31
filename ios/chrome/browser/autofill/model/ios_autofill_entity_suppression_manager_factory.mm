// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_entity_suppression_manager_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/in_memory_entity_suppression_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
autofill::EntitySuppressionManager*
IOSAutofillEntitySuppressionManagerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<autofill::EntitySuppressionManager>(
          profile, /*create=*/true);
}

// static
IOSAutofillEntitySuppressionManagerFactory*
IOSAutofillEntitySuppressionManagerFactory::GetInstance() {
  static base::NoDestructor<IOSAutofillEntitySuppressionManagerFactory>
      instance;
  return instance.get();
}

IOSAutofillEntitySuppressionManagerFactory::
    IOSAutofillEntitySuppressionManagerFactory()
    : ProfileKeyedServiceFactoryIOS("EntitySuppressionManager",
                                    ProfileSelection::kNoInstanceInIncognito) {}

IOSAutofillEntitySuppressionManagerFactory::
    ~IOSAutofillEntitySuppressionManagerFactory() = default;

std::unique_ptr<KeyedService>
IOSAutofillEntitySuppressionManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAmbientAutofillSuppression)) {
    return nullptr;
  }

  return std::make_unique<autofill::InMemoryEntitySuppressionManager>();
}
