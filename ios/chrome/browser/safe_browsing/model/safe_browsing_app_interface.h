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

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_APP_INTERFACE_H_
