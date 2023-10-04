// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_FETCHER_TESTING_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_FETCHER_TESTING_H_

#import <memory>

namespace variations {
struct SeedResponse;
}  // namespace variations

// Extraction of private properties and methods in
// IOSChromeVariationsSeedFetcher to be tested.
@interface IOSChromeVariationsSeedFetcher (Testing)

- (void)doActualFetch;

- (void)seedRequestDidCompleteWithData:(NSData*)data
                              response:(NSHTTPURLResponse*)httpResponse
                                 error:(NSError*)error;

- (std::unique_ptr<variations::SeedResponse>)
    seedResponseForHTTPResponse:(NSHTTPURLResponse*)httpResponse
                           data:(NSData*)data;

+ (void)resetFetchingStatusForTesting;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_FETCHER_TESTING_H_
