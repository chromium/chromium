// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/ios_chrome_variations_seed_store.h"

#import "base/no_destructor.h"
#import "components/variations/seed_response.h"
#import "ios/chrome/browser/variations/ios_chrome_variations_seed_store+fetcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Swaps `seed` with the shared seed.
std::unique_ptr<variations::SeedResponse> SwapSeedWithSharedSeed(
    std::unique_ptr<variations::SeedResponse> seed) {
  // The source of truth of the entire app for the stored seed.
  static base::NoDestructor<std::unique_ptr<variations::SeedResponse>>
      g_shared_seed;
  std::swap(*g_shared_seed, seed);
  return seed;
}
}  // namespace

@implementation IOSChromeVariationsSeedStore

#pragma mark - Public

+ (std::unique_ptr<variations::SeedResponse>)popSeed {
  // The old global seed is returned after being replaced
  // with nullptr.
  return SwapSeedWithSharedSeed(nullptr);
}

#pragma mark - Private Setter (exposed to fetcher only)

// Updates the shared seed response.
+ (void)updateSharedSeed:(std::unique_ptr<variations::SeedResponse>)seed {
  // The old global seed is returned and destroyed when
  // the returned object goes out of scope.
  SwapSeedWithSharedSeed(std::move(seed));
}

@end
