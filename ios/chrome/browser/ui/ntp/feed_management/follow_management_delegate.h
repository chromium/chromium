// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_DELEGATE_H_

#import <Foundation/Foundation.h>

@class WebChannel;

// Actions taken on the follow management UI.
@protocol FollowManagementDelegate

// User requested to unfollow a WebChannel.
- (void)unfollowWebChannel:(WebChannel*)channel
                completion:(void (^)(BOOL success))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_DELEGATE_H_
