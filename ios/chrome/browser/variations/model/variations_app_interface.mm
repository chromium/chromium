// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/variations_app_interface.h"

#import <string>

#import "base/base64.h"
#import "base/check_op.h"
#import "base/test/bind.h"
#import "components/prefs/pref_service.h"
#import "components/variations/metrics.h"
#import "components/variations/pref_names.h"
#import "components/variations/seed_reader_writer.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/variations_test_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

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

  prefService->ClearPref(variations::prefs::kVariationsSafeSeedLocale);
  prefService->ClearPref(
      variations::prefs::kVariationsPermanentOverriddenCountry);

  // Clear variations policy prefs.
  prefService->ClearPref(variations::prefs::kVariationsRestrictionsByPolicy);
  prefService->ClearPref(variations::prefs::kVariationsRestrictParameter);

  // Clear prefs that may trigger variations safe mode.
  prefService->ClearPref(variations::prefs::kVariationsCrashStreak);
  prefService->ClearPref(variations::prefs::kVariationsFailedToFetchSeedStreak);

  // Clear other prefs before queued safe seed readers can run. The regular seed
  // reader (`GetSeedReaderWriter()`) is eager, so clear the safe seed reader
  // (`GetSafeSeedReaderWriter()`) last so its callbacks see the fully cleared
  // test state.
  GetSeedReaderWriter()->ClearAllSeedInfo();
  GetSafeSeedReaderWriter()->ClearAllSeedInfo();
}

+ (BOOL)fieldTrialExistsForTestSeed {
  return variations::FieldTrialListHasAllStudiesFrom(
      variations::TestSeedData());
}

+ (void)hasSafeSeed:(void (^)(BOOL hasSeed))completion {
  GetSafeSeedReaderWriter()->ReadSeedData(base::BindLambdaForTesting(
      [completion](variations::SeedReaderWriter::ReadSeedDataResult result) {
        BOOL hasSeed = (result.result != variations::LoadSeedResult::kEmpty);
        completion(hasSeed);
      }));
}

+ (void)setTestSafeSeedAndSignature {
  // Clearing also initializes the deferred reader before storing the test seed.
  GetSafeSeedReaderWriter()->ClearAllSeedInfo();
  std::string seed_data;
  base::Base64Decode(variations::TestSeedData().base64_uncompressed_data,
                     &seed_data);
  const variations::StoreSeedResult result =
      GetSafeSeedReaderWriter()->StoreValidatedSeedInfo(
          variations::ValidatedSeedInfo{
              .seed_data = seed_data,
              .signature = variations::TestSeedData().base64_signature,
              .milestone = 92,  // Milestone number is arbitrary.
              .seed_date = base::Time::Now(),
              .client_fetch_time = base::Time::Now(),
              .session_country_code = "us",
              .permanent_country_code = "us",
              // Permanent version is not stored in the safe seed, only the
              // country.
              .permanent_country_version = "",
          });
  CHECK_EQ(result, variations::StoreSeedResult::kSuccess);
}

+ (void)setCrashingRegularSeedAndSignature {
  std::string seed_data;
  base::Base64Decode(variations::CrashingSeedData().base64_uncompressed_data,
                     &seed_data);
  GetSeedReaderWriter()->StoreValidatedSeedInfo(variations::ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = variations::CrashingSeedData().base64_signature,
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
