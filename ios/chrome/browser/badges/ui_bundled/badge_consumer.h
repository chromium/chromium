// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol BadgeItem;
class InfoBarIOS;

// Consumer protocol for the view controller that displays badges.
@protocol BadgeConsumer <NSObject>
// Notifies the consumer to reset with `displayedBadgeItem` and
// `fullscreenBadgeItem`.
- (void)setupWithDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
                fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem;
// Notifies the consumer to update its badges with the configurations of
// `displayedBadgeItem` and `fullscreenBadgeItem` with the use of `infoBar`,
// if required.
- (void)updateDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
             fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem
                     infoBar:(InfoBarIOS*)infoBar;
// Notifies the consumer whether or not there are unread badges. See
// BadgeStateRead for more information.
- (void)markDisplayedBadgeAsRead:(BOOL)read;

@end

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_CONSUMER_H_
