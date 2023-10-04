// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_FETCHER_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_FETCHER_H_

#import <Foundation/Foundation.h>

// Protocol for variations seed fetcher that reacts to variations seed fetch
// stages.
@protocol IOSChromeVariationsSeedFetcherDelegate

// Informs the delegate that the initial seed fetch has completed.
// If the value of the `success` parameter is YES, the seed fetch is successful;
// if NO, the fetch has failed.
- (void)variationsSeedFetcherDidCompleteFetchWithSuccess:(BOOL)success;

@end

// An object that allows its owner to fetch variations seed before Chrome
// components are initialized.
@interface IOSChromeVariationsSeedFetcher : NSObject

// Delegate object that would be informed when the seed fetch completes.
@property(nonatomic, weak) id<IOSChromeVariationsSeedFetcherDelegate> delegate;

// Initializes the seed fetcher using `arguments` as variation switches, and
// apply them to the seed manager.
//
// Note: If the user calls `init`, the fetcher
// would be initialized with command line arguments.
- (instancetype)initWithArguments:(NSArray<NSString*>*)arguments
    NS_DESIGNATED_INITIALIZER;

// Starts fetching the initial seed from the variations server. The `delegate`
// property would be informed when the fetch completes. If the fetch is
// successful, the acquired seed would be stored in
// IOSChromeVariationsSeedStore.
//
// Note: the caller is responsible for making sure that a seed fetcher object is
// only be initiated when there is no valid variations seed available in local
// storage. In cases when this method is invoked when a variations seed is
// available, the downloaded seed would be disregarded.
- (void)startSeedFetch;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_IOS_CHROME_VARIATIONS_SEED_FETCHER_H_
