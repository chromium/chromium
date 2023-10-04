// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_FETCHER_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_FETCHER_H_

// Extraction of seed update method in IOSChromeVariationsSeedStore to
// be used (and ONLY used) by the fetcher.
@interface IOSChromeVariationsSeedStore (Fetcher)

+ (void)updateSharedSeed:(std::unique_ptr<variations::SeedResponse>)seed;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_STORE_FETCHER_H_
