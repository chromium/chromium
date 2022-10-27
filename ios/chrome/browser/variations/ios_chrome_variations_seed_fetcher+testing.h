// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_FETCHER_TESTING_H_
#define IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_FETCHER_TESTING_H_

// Extraction of private properties and methods in
// IOSChromeVariationsSeedFetcher to be tested.
@interface IOSChromeVariationsSeedFetcher (Testing)

@property(nonatomic, readonly) NSURL* variationsUrl;

@property(nonatomic, strong) NSDate* startTimeOfOngoingSeedRequest;

- (void)applySwitchesFromArguments:(NSArray<NSString*>*)arguments;

- (void)onSeedRequestCompletedWithData:(NSData*)data
                              response:(NSHTTPURLResponse*)httpResponse
                                 error:(NSError*)error;

- (IOSChromeSeedResponse*)seedResponseForHTTPResponse:
                              (NSHTTPURLResponse*)httpResponse
                                                 data:(NSData*)data;

+ (void)resetFetchingStatusForTesting;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_FETCHER_TESTING_H_
