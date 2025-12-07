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

// Whether the badge view controller is in force disable mode. While force
// disabled, the displayed badge remains hidden.
@property(nonatomic, assign) BOOL forceDisabled;

// Notifies the consumer to reset the `displayedBadgeItem`.
- (void)setupWithDisplayedBadge:(id<BadgeItem>)displayedBadgeItem;
// Notifies the consumer to update its badges with the configuration of
// `displayedBadgeItem` with the use of `infoBar`, if required.
- (void)updateDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
                     infoBar:(InfoBarIOS*)infoBar;
// Notifies the consumer whether or not there are unread badges. See
// BadgeStateRead for more information.
- (void)markDisplayedBadgeAsRead:(BOOL)read;
// Updates the displayed badges with the complete list of badges to show.
- (void)updateDisplayedBadges:(NSArray<id<BadgeItem>>*)badgesToDisplay;

@end

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_CONSUMER_H_
