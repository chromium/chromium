// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"

#import <optional>

#import "base/check.h"
#import "base/no_destructor.h"
#import "base/notreached.h"
#import "components/variations/seed_response.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+fetcher.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+testing.h"

namespace {

using ::variations::SeedApplicationStage;
using ::variations::SeedResponse;

// Initial seed application stage.
constexpr SeedApplicationStage kInitialStage = SeedApplicationStage::kNoSeed;

// Returns the seed application stage following `stage`.
constexpr std::optional<SeedApplicationStage> NextStage(
    SeedApplicationStage stage) {
  switch (stage) {
    case SeedApplicationStage::kNoSeed:
      return SeedApplicationStage::kSeedStored;

    case SeedApplicationStage::kSeedStored:
      return SeedApplicationStage::kSeedImported;

    case SeedApplicationStage::kSeedImported:
      return SeedApplicationStage::kSeedApplied;

    case SeedApplicationStage::kSeedApplied:
      return std::nullopt;
  }

  NOTREACHED();
}

// Stores the current seed application stage.
SeedApplicationStage g_current_stage = kInitialStage;

// Sets the current application stage to `next_stage` if currently at `stage`.
inline void IncrementSeedApplicationStageIfAt(SeedApplicationStage stage) {
  std::optional<SeedApplicationStage> next_stage = NextStage(stage);
  CHECK(next_stage.has_value());

  if (g_current_stage == stage) {
    g_current_stage = next_stage.value();
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
  return g_current_stage;
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
  g_current_stage = kInitialStage;
}

@end
