// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_DELEGATE_H_

@class FollowManagementViewController;

// Events from the Follow Management UI.
@protocol FollowManagementViewDelegate

// Called when the follow management UI will be dismissed.
- (void)followManagementViewControllerWillDismiss:
    (FollowManagementViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_DELEGATE_H_
