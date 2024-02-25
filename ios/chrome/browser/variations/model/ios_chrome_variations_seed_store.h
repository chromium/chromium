// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_H_

#import <Foundation/Foundation.h>
#import <memory>

namespace variations {

struct SeedResponse;

// The current initialization stage of the variations seed fetched by
// IOSChromeVariationsSeedFetcher. This should be mapped to
// `VariationsFirstRunSeedApplicationStage` in enum.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SeedApplicationStage {
  kNoSeed = 0,
  kSeedStored = 1,
  kSeedImported = 2,
  kSeedApplied = 3,
  kMaxValue = kSeedApplied,
};

}  // namespace variations

// Storage for variations seed fetched by
// IOSChromeVariationsSeedFetcher.
@interface IOSChromeVariationsSeedStore : NSObject

// Returns the seed response and resets it; called by the variations service to
// import the seed into Chrome Prefs.
+ (std::unique_ptr<variations::SeedResponse>)popSeed;

// Notifies the seed store that a seed, which may or may not be imported from
// the iOS seed store, has been applied to create field trials.
+ (void)notifySeedApplication;

// Returns the progress of application of the seed in the seed store, if exists.
// Returns kNoSeed if no seed has been stored.
+ (variations::SeedApplicationStage)seedApplicationStage;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_H_
