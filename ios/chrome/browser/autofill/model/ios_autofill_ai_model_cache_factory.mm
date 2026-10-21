// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_ai_model_cache_factory.h"

#import <utility>

#import "base/feature_list.h"
#import "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache_impl.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

IOSAutofillAiModelCacheFactory::IOSAutofillAiModelCacheFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AutofillAiModelCache",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSAutofillAiModelCacheFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiServerModel)) {
    return nullptr;
  }

  return std::make_unique<autofill::AutofillAiModelCacheImpl>(
      ios::HistoryServiceFactory::GetForProfile(profile),
      profile->GetProtoDatabaseProvider(), profile->GetStatePath(),
      autofill::features::kAutofillAiServerModelCacheSize.Get(),
      autofill::features::kAutofillAiServerModelCacheAge.Get());
}
