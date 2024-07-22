// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh_constants.h"

// TODO(crbug.com/349339414): Remove feed related tasks once a generic solution
// will be implemented (chrome.app.refresh).
NSString* const kFeedBackgroundRefreshTaskIdentifier = @"chrome.feed.refresh";

NSString* const kFeedLastBackgroundRefreshTimestamp =
    @"FeedLastBackgroundRefreshTimestamp";

NSString* const kAppBackgroundRefreshTaskIdentifier = @"chrome.app.refresh";
