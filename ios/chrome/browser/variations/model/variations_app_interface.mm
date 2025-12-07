// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/variations_app_interface.h"

#import <string>

#import "base/base64.h"
#import "base/metrics/field_trial.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/threading/thread.h"
#import "components/prefs/pref_service.h"
#import "components/variations/pref_names.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/variations_test_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace {

variations::SeedReaderWriter* GetSeedReaderWriter() {
  return GetApplicationContext()
      ->GetVariationsService()
      ->GetSeedStoreForTesting()
      ->GetSeedReaderWriterForTesting();
}

variations::SeedReaderWriter* GetSafeSeedReaderWriter() {
  return GetApplicationContext()
      ->GetVariationsService()
      ->GetSeedStoreForTesting()
      ->GetSafeSeedReaderWriterForTesting();
}

}  // namespace

@implementation VariationsAppInterface

+ (void)clearVariationsPrefs {
  PrefService* prefService = GetApplicationContext()->GetLocalState();

  // Clear variations seed prefs.
  GetSeedReaderWriter()->ClearSeedInfo();
  // Here session country is cleared for testing, but it should not be cleared
  // for the regular seed.
  GetSeedReaderWriter()->ClearSessionCountry();
  GetApplicationContext()
      ->GetVariationsService()
      ->GetSeedStoreForTesting()
      ->ClearPermanentConsistencyCountryAndVersion();
  prefService->ClearPref(
      variations::prefs::kVariationsPermanentOverriddenCountry);

  // Clear variations safe seed prefs.
  GetSafeSeedReaderWriter()->ClearSeedInfo();
  GetSafeSeedReaderWriter()->ClearSessionCountry();
  GetSafeSeedReaderWriter()->ClearPermanentConsistencyCountryAndVersion();
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

+ (void)hasSafeSeed:(void (^)(BOOL hasSeed))completion {
  GetSafeSeedReaderWriter()->ReadSeedData(base::BindLambdaForTesting(
      [completion](variations::SeedReaderWriter::ReadSeedDataResult result) {
        BOOL hasSeed = (result.result != variations::LoadSeedResult::kEmpty);
        completion(hasSeed);
      }));
}

+ (void)setTestSafeSeedAndSignature {
  std::string seed_data;
  base::Base64Decode(variations::kTestSeedData.base64_uncompressed_data,
                     &seed_data);
  GetSafeSeedReaderWriter()->StoreValidatedSeedInfo(
      variations::ValidatedSeedInfo{
          .seed_data = seed_data,
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
  std::string seed_data;
  base::Base64Decode(variations::kCrashingSeedData.base64_uncompressed_data,
                     &seed_data);
  GetSeedReaderWriter()->StoreValidatedSeedInfo(variations::ValidatedSeedInfo{
      .seed_data = seed_data,
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
