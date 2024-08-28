// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_view_controller_delegate.h"

@protocol NewTabPageDelegate;

// View Controller that contains all the elements of the Feed Top section.
@interface FeedTopSectionViewController
    : UIViewController <FeedTopSectionConsumer>

// Delegate to handle interactions related to children views.
@property(nonatomic, weak) id<FeedTopSectionViewControllerDelegate> delegate;

// Delegate to handle interactions of the signin promo.
@property(nonatomic, weak) id<SigninPromoViewDelegate> signinPromoDelegate;

// Delegate to handle interactions of the notifications promo.
@property(nonatomic, weak) id<FeedTopSectionMutator> feedTopSectionMutator;

// Delegate for NTP related actions.
@property(nonatomic, weak) id<NewTabPageDelegate> NTPDelegate;

// Returns |YES| if the promo is currently in the feed, whether or not it is
// visible.
@property(nonatomic, assign) BOOL shouldShowSigninPromo;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_TOP_SECTION_FEED_TOP_SECTION_VIEW_CONTROLLER_H_
