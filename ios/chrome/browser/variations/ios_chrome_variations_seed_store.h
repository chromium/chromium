// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_STORE_H_
#define IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_STORE_H_

#import <Foundation/Foundation.h>

@class IOSChromeSeedResponse;

// Storage for variations seed fetched by
// IOSChromeVariationsSeedFetcher.
@interface IOSChromeVariationsSeedStore : NSObject

// Returns the seed response and resets it; called by the variations service to
// import the seed into Chrome Prefs.
+ (IOSChromeSeedResponse*)popSeed;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_STORE_H_
