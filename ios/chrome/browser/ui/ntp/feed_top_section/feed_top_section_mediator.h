// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller_delegate.h"

class ChromeBrowserState;
@protocol FeedTopSectionConsumer;
@protocol NewTabPageDelegate;
@class SigninPromoViewMediator;

// Mediator for the NTP Feed top section, handling the interactions.
@interface FeedTopSectionMediator
    : NSObject <FeedTopSectionViewControllerDelegate, SigninPromoViewConsumer>

// TODO(crbug.com/1331010): Mediators shouldn't know about browser state. We
// should have the coordinator provide any necessary information.
- (instancetype)initWithConsumer:(id<FeedTopSectionConsumer>)consumer
                    browserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The mediator handling the interactions of the signin promo.
@property(nonatomic, strong) SigninPromoViewMediator* signinPromoMediator;

// Delegate for NTP related actions.
@property(nonatomic, weak) id<NewTabPageDelegate> ntpDelegate;

// Initializes the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_FEED_TOP_SECTION_MEDIATOR_H_
