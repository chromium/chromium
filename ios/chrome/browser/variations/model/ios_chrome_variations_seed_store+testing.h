// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_TESTING_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_TESTING_H_

#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"

// Unit test interface for IOSChromeVariationsSeedStore.
@interface IOSChromeVariationsSeedStore (Testing)

// Clears global variables. Use in unit tests only.
+ (void)resetForTesting;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_TESTING_H_
