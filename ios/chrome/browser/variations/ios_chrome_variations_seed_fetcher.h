// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_FETCHER_H_
#define IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_FETCHER_H_

#import <Foundation/Foundation.h>

@class IOSChromeSeedResponse;

// Protocol for variations seed fetcher that reacts to variations seed fetch
// stages.
@protocol IOSChromeVariationsSeedFetcherDelegate

// Informs the delegate that the initial seed fetch has successfully completed
// or failed.
- (void)didFetchSeedSuccess:(BOOL)succeeded;

@end

// An object that allows its owner to fetch variations seed before Chrome
// components are initialized.
@interface IOSChromeVariationsSeedFetcher : NSObject

// Delegate object that observes the status of seed fetching.
@property(nonatomic, weak) id<IOSChromeVariationsSeedFetcherDelegate> delegate;

// Starts fetching the initial seed from the variations server.
//
// Note: the caller is responsible for making sure that a seed fetcher object is
// only be initiated when there is no valid variations seed available in local
// storage. In cases when this method is invoked when a variations seed is
// available, the downloaded seed would be disregarded.
- (void)startSeedFetch;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_VARIATIONS_SEED_FETCHER_H_
