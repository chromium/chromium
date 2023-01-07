// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_NAVIGATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_NAVIGATION_DELEGATE_H_

class GURL;

// Delegate for handling navigation actions.
@protocol FeedManagementNavigationDelegate

// Navigate to activity.
- (void)handleNavigateToActivity;

// Navigate to interests.
- (void)handleNavigateToInterests;

// Navigate to hidden.
- (void)handleNavigateToHidden;

// Navigate to `url` of a followed site.
- (void)handleNavigateToFollowedURL:(const GURL&)url;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_NAVIGATION_DELEGATE_H_
