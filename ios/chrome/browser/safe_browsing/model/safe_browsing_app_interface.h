// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// App interface for Safe Browsing tests.
@interface SafeBrowsingAppInterface : NSObject

// Sets a mock scorer in ClientSideDetectionService for the original profile.
+ (void)setMockScorer;

// Returns YES if the scorer is set in ClientSideDetectionService for the
// original profile.
+ (BOOL)isScorerSet;

// Clears the scorer in ClientSideDetectionService for the original profile.
+ (void)clearScorer;

// Returns YES if ClientSideDetectionFeatureCache has a cached verdict for
// the given `url`.
+ (BOOL)hasCachedVerdictForURL:(NSString*)url;

// Caches an artificial real-time URL verdict with the given `url` and force
// request status.
+ (void)cacheRealTimeVerdictForURL:(NSString*)url
                      forceRequest:(BOOL)forceRequest;

// Triggers the completion of visual classification with `scores`.
+ (void)triggerClassificationDoneWithURL:(NSString*)url
                            visualScores:(NSArray<NSNumber*>*)scores;

// Returns the cached real-time URL `ClientSideDetectionType` for the given
// `url`.
+ (NSInteger)cachedRealTimeURLClientSideDetectionTypeForURL:(NSString*)url;

// Sets whether the local resource / localhost pre-classification check should
// be bypassed for testing.
+ (void)setBypassLocalResourceCheckForTesting:(BOOL)bypass;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_APP_INTERFACE_H_
