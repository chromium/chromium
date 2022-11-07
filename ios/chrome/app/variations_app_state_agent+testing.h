// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_TESTING_H_
#define IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_TESTING_H_

@class IOSChromeVariationsSeedFetcher;

@interface VariationsAppStateAgent (Testing)

// Initializer that takes an existing fetcher and whether the feature that
// fetches seed on first run should be enabled.
- (instancetype)initWithFirstRunStatus:(BOOL)firstRun
                               fetcher:(IOSChromeVariationsSeedFetcher*)fetcher
                        featureEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_TESTING_H_
