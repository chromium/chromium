// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_MANAGEMENT_FEED_MANAGEMENT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_MANAGEMENT_FEED_MANAGEMENT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol FeedManagementNavigationDelegate;
@class FeedMetricsRecorder;

// The top-level owner of the Feed Management component. It serves to connect
// the various independent pieces such as the Feed Management UI, the Follow
// Management UI, and mediators.
@interface FeedManagementCoordinator : ChromeCoordinator

// Delegate for handling web navigation actions.
@property(nonatomic, weak) id<FeedManagementNavigationDelegate>
    navigationDelegate;

// Feed metrics recorder.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_MANAGEMENT_FEED_MANAGEMENT_COORDINATOR_H_
