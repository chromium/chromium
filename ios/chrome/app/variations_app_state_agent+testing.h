// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_TESTING_H_
#define IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_TESTING_H_

namespace base {
class Time;
}  // namespace base
@class IOSChromeVariationsSeedFetcher;

// Name of current experiment.
extern const char kIOSChromeVariationsTrialName[];

// Name for the default group.
extern const char kIOSChromeVariationsTrialDefaultGroup[];
// Name for the control group.
extern const char kIOSChromeVariationsTrialControlGroup[];
// Name for the enabled group.
extern const char kIOSChromeVariationsTrialEnabledGroup[];

// Histogram name for seed expiry.
extern const char kIOSSeedExpiryHistogram[];

@interface VariationsAppStateAgent (Testing)

// Initializer that takes an existing fetcher and the enabled and control
// weights to turn on the feature that fetches seed on first run should be
// enabled.
- (instancetype)initWithFirstRunExperience:(BOOL)shouldPresentFRE
                         lastSeedFetchTime:(base::Time)lastSeedFetchTime
                                   fetcher:
                                       (IOSChromeVariationsSeedFetcher*)fetcher
                        enabledGroupWeight:(int)enabledGroupWeight
                        controlGroupWeight:(int)controlGroupWeight;

@end

#endif  // IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_TESTING_H_
