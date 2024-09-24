// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_PROFILE_HELPER_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_PROFILE_HELPER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "base/time/time.h"

// Callback invoked with the result of a background refresh operation.
using BackgroundRefreshCallback = base::OnceCallback<void(bool success)>;

// Protocol representing the per-Profile operations delegated by the
// DiscoverFeedAppAgent.
@protocol DiscoverFeedProfileHelper <NSObject>

// Invoked when the DiscoverFeedAppAgent wants the feed for the Profile
// to be refreshed (usually when the application enter background).
- (void)refreshFeedInBackground;

// Invoked when the DiscoverFeedAppAgent wants the feed for the Profile
// to be refreshed as a background operation. Must call `callback` with
// the result of the operation when completed.
- (void)performBackgroundRefreshes:(BackgroundRefreshCallback)callback;

// Invoked when the DiscoverFeedAppAgent wants to cancel a background
// refresh operation.
- (void)handleBackgroundRefreshTaskExpiration;

// Invoked by the DiscoverFeedAppAgent to get the time of the next refresh
// operation for this Profile.
- (base::Time)earliestBackgroundRefreshDate;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_PROFILE_HELPER_H_
