// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"

#import "base/no_destructor.h"
#import "components/variations/seed_response.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+fetcher.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+testing.h"

namespace {

using ::variations::SeedApplicationStage;
using ::variations::SeedResponse;

// Seed application stages in the order of sequence they should happen.
const SeedApplicationStage kSeedApplicationStages[] = {
    SeedApplicationStage::kNoSeed, SeedApplicationStage::kSeedStored,
    SeedApplicationStage::kSeedImported, SeedApplicationStage::kSeedApplied};

// Convenience getter and setter for the seed application stage. If
// `increment=true`, increment the stage before returning; if
// `reset_for_testing=true`, the seed application stage would be reset to
// `kNoSeed`.
//
// NOTE:`reset_for_testing` has precedence over `increment`, and setting this
// parameter explicitly should ONLY be done for testing purpose.
SeedApplicationStage SeedApplicationStageAccessor(
    bool increment,
    bool reset_for_testing = false) {
  // Index of the current seed application stage in `kSeedApplicationStages`.
  static int g_shared_seed_application_stage_idx = 0;

  if (reset_for_testing) {
    g_shared_seed_application_stage_idx = 0;
  } else if (increment) {
    g_shared_seed_application_stage_idx++;
  }
  CHECK_LE(g_shared_seed_application_stage_idx,
           static_cast<int>(SeedApplicationStage::kMaxValue));
  return kSeedApplicationStages[g_shared_seed_application_stage_idx];
}

// Getter for seed application stage extracted for readability purpose.
SeedApplicationStage GetSeedApplicationStage() {
  return SeedApplicationStageAccessor(/*increment=*/false);
}

// Increment the seed application stage if the current stage equals `stage`,
// otherwise do nothing; extracted for readability purpose.
void IncrementSeedApplicationStageIfAt(SeedApplicationStage stage) {
  if (GetSeedApplicationStage() == stage) {
    SeedApplicationStageAccessor(/*increment=*/true);
  }
}

// Swaps `seed` with the shared seed.
std::unique_ptr<SeedResponse> SwapSeedWithSharedSeed(
    std::unique_ptr<SeedResponse> seed) {
  // The source of truth of the entire app for the stored seed.
  static base::NoDestructor<std::unique_ptr<SeedResponse>> g_shared_seed;
  std::swap(*g_shared_seed, seed);
  return seed;
}

}  // namespace

@implementation IOSChromeVariationsSeedStore

#pragma mark - Public

+ (std::unique_ptr<SeedResponse>)popSeed {
  IncrementSeedApplicationStageIfAt(SeedApplicationStage::kSeedStored);
  // The old global seed is returned after being replaced
  // with nullptr.
  return SwapSeedWithSharedSeed(nullptr);
}

+ (void)notifySeedApplication {
  // If the seed has never been imported by variations service, the applied seed
  // is highly unlikely to be the seed that was in the seed store. Skip
  // incrementing.
  IncrementSeedApplicationStageIfAt(SeedApplicationStage::kSeedImported);
}

+ (SeedApplicationStage)seedApplicationStage {
  return GetSeedApplicationStage();
}

#pragma mark - Private Setter (exposed to fetcher only)

// Updates the shared seed response.
+ (void)updateSharedSeed:(std::unique_ptr<SeedResponse>)seed {
  // The old global seed is returned and destroyed when
  // the returned object goes out of scope.
  SwapSeedWithSharedSeed(std::move(seed));
  IncrementSeedApplicationStageIfAt(SeedApplicationStage::kNoSeed);
}

#pragma mark - Testing only

+ (void)resetForTesting {
  SwapSeedWithSharedSeed(nullptr);
  SeedApplicationStageAccessor(false, /*reset_for_testing=*/true);
}

@end
