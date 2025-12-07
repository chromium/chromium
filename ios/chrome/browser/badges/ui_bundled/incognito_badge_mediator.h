// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol IncognitoBadgeConsumer;
class WebStateList;

// A mediator object that updates the consumer when the state of badges changes.
@interface IncognitoBadgeMediator : NSObject

// The consumer being set up by this mediator. Setting to a new value updates
// the new consumer.
@property(nonatomic, weak) id<IncognitoBadgeConsumer> consumer;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_MEDIATOR_H_
