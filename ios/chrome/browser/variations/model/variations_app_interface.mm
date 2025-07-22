// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/variations_app_interface.h"

#import <string>

#import "base/metrics/field_trial.h"
#import "components/prefs/pref_service.h"
#import "components/variations/pref_names.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/variations_test_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation VariationsAppInterface

+ (void)clearVariationsPrefs {
  PrefService* prefService = GetApplicationContext()->GetLocalState();

  // Clear variations seed prefs.
  variations::VariationsSeedStore* seed_store =
      GetApplicationContext()->GetVariationsService()->GetSeedStoreForTesting();
  seed_store->GetSeedReaderWriterForTesting()->ClearSeedInfo();
  // Here session country is cleared for testing, but it should not be cleared
  // for the regular seed.
  seed_store->GetSeedReaderWriterForTesting()->ClearSessionCountry();
  seed_store->ClearPermanentConsistencyCountryAndVersion();
  prefService->ClearPref(
      variations::prefs::kVariationsPermanentOverriddenCountry);

  // Clear variations safe seed prefs.
  seed_store->GetSafeSeedReaderWriterForTesting()->ClearSeedInfo();
  seed_store->GetSafeSeedReaderWriterForTesting()->ClearSessionCountry();
  seed_store->GetSafeSeedReaderWriterForTesting()
      ->ClearPermanentConsistencyCountryAndVersion();
  prefService->ClearPref(variations::prefs::kVariationsSafeSeedLocale);

  // Clear variations policy prefs.
  prefService->ClearPref(variations::prefs::kVariationsRestrictionsByPolicy);
  prefService->ClearPref(variations::prefs::kVariationsRestrictParameter);

  // Clear prefs that may trigger variations safe mode.
  prefService->ClearPref(variations::prefs::kVariationsCrashStreak);
  prefService->ClearPref(variations::prefs::kVariationsFailedToFetchSeedStreak);
}

+ (BOOL)fieldTrialExistsForTestSeed {
  return variations::FieldTrialListHasAllStudiesFrom(variations::kTestSeedData);
}

+ (BOOL)hasSafeSeed {
  return !GetApplicationContext()
              ->GetVariationsService()
              ->GetSeedStoreForTesting()
              ->GetSafeSeedReaderWriterForTesting()
              ->GetSeedData()
              .data.empty();
}

+ (void)setTestSafeSeedAndSignature {
  GetApplicationContext()
      ->GetVariationsService()
      ->GetSeedStoreForTesting()
      ->GetSafeSeedReaderWriterForTesting()
      ->StoreValidatedSeedInfo(variations::ValidatedSeedInfo{
          .compressed_seed_data = variations::kTestSeedData.GetCompressedData(),
          .base64_seed_data = variations::kTestSeedData.base64_compressed_data,
          .signature = variations::kTestSeedData.base64_signature,
          .milestone = 92,  // Milestone number is arbitrary.
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
          .permanent_country_code = "us",
          // Permanent version is not stored in the safe seed, only the country.
          .permanent_country_version = "",
      });
}

+ (void)setCrashingRegularSeedAndSignature {
  GetApplicationContext()
      ->GetVariationsService()
      ->GetSeedStoreForTesting()
      ->GetSeedReaderWriterForTesting()
      ->StoreValidatedSeedInfo(variations::ValidatedSeedInfo{
          .compressed_seed_data =
              variations::kCrashingSeedData.GetCompressedData(),
          .base64_seed_data =
              variations::kCrashingSeedData.base64_compressed_data,
          .signature = variations::kCrashingSeedData.base64_signature,
          .milestone = 92,  // Milestone number is arbitrary.
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
          .permanent_country_code = "us",
          .permanent_country_version = "1.2.3.4",
      });
}

+ (int)crashStreak {
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  return prefService->GetInteger(variations::prefs::kVariationsCrashStreak);
}

+ (void)setCrashValue:(int)value {
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  prefService->SetInteger(variations::prefs::kVariationsCrashStreak, value);
}

+ (int)failedFetchStreak {
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  return prefService->GetInteger(
      variations::prefs::kVariationsFailedToFetchSeedStreak);
}

+ (void)setFetchFailureValue:(int)value {
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  prefService->SetInteger(variations::prefs::kVariationsFailedToFetchSeedStreak,
                          value);
}

@end
