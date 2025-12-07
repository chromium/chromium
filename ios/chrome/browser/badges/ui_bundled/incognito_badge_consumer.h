// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol BadgeItem;

// Consumer protocol for the view controller that displays a fullscreen badge.
@protocol IncognitoBadgeConsumer <NSObject>

// Whether the badge view controller is disabled.
@property(nonatomic, assign) BOOL disabled;

// Notifies the consumer to reset with `incognitoBadgeItem`.
- (void)setupWithIncognitoBadge:(id<BadgeItem>)incognitoBadgeItem;

@end

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_CONSUMER_H_
