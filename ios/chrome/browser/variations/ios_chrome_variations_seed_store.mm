// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/ios_chrome_variations_seed_store.h"

#import "base/no_destructor.h"
#import "components/variations/seed_response.h"
#import "ios/chrome/browser/variations/ios_chrome_variations_seed_store+fetcher.h"
#import "ios/chrome/browser/variations/ios_chrome_variations_seed_store+testing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ::variations::SeedApplicationStage;
using ::variations::SeedResponse;

// Seed application stages in the order of sequence they should happen.
const SeedApplicationStage kSeedApplicationStages[] = {
    SeedApplicationStage::kNoSeed, SeedApplicationStage::kSeedStored,
    SeedApplicationStage::kSeedImported, SeedApplicationStage::kSeedApplied};

// Convenience getter and setter for the seed application stage. If
// `increment=true`, increment the stage before returning.
SeedApplicationStage SeedApplicationStageAccessor(bool increment) {
  static int g_shared_seed_application_stage_idx = 0;
  if (increment) {
    g_shared_seed_application_stage_idx += 1;
    // Incrementing the final stage will reset the stage to `kNoSeed`.
    if (g_shared_seed_application_stage_idx >
        static_cast<int>(SeedApplicationStage::kMaxValue)) {
      g_shared_seed_application_stage_idx = 0;
    }
  }
  return kSeedApplicationStages[g_shared_seed_application_stage_idx];
}

// Getter for seed application stage extracted for readability purpose.
SeedApplicationStage GetSeedApplicationStage() {
  return SeedApplicationStageAccessor(false);
}

// Incrementer for seed application stage extracted for readability purpose.
void IncrementSeedApplicationStage() {
  SeedApplicationStageAccessor(true);
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
  if (GetSeedApplicationStage() == SeedApplicationStage::kSeedStored) {
    IncrementSeedApplicationStage();
  }
  // The old global seed is returned after being replaced
  // with nullptr.
  return SwapSeedWithSharedSeed(nullptr);
}

+ (void)notifySeedApplication {
  // If the seed has never been imported by variations service, the applied seed
  // is highly unlikely to be the seed that was in the seed store. Skip
  // incrementing.
  if (GetSeedApplicationStage() == SeedApplicationStage::kSeedImported) {
    IncrementSeedApplicationStage();
  }
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
  if (GetSeedApplicationStage() == SeedApplicationStage::kNoSeed) {
    IncrementSeedApplicationStage();
  }
}

+ (void)resetForTesting {
  SwapSeedWithSharedSeed(nullptr);
  while (SeedApplicationStageAccessor(NO) != SeedApplicationStage::kNoSeed) {
    SeedApplicationStageAccessor(YES);
  }
}

@end
