// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/ios_chrome_variations_seed_store.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The source of truth of the entire app for the stored seed.
static IOSChromeSeedResponse* g_shared_seed = nil;
}  // namespace

@implementation IOSChromeVariationsSeedStore

#pragma mark - Public

+ (IOSChromeSeedResponse*)popSeed {
  IOSChromeSeedResponse* seed = g_shared_seed;
  g_shared_seed = nil;
  return seed;
}

#pragma mark - Private Setter (exposed to fetcher only)

// Updates the shared seed response.
+ (void)updateSharedSeed:(IOSChromeSeedResponse*)seed {
  g_shared_seed = seed;
}

@end
