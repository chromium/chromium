// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/discover_feed/feed_constants.h"

@protocol FeedControlDelegate;
@protocol NewTabPageDelegate;

@interface FeedHeaderViewController : UIViewController

// Button for opening top-level feed menu.
@property(nonatomic, readonly, strong) UIButton* menuButton;

// The base title string of the feed header, excluding modifiers.
@property(nonatomic, copy) NSString* titleText;

// Delegate for controlling the presented feed.
@property(nonatomic, weak) id<FeedControlDelegate> feedControlDelegate;

// Delegate to communicate back to the New Tab Page coordinator.
@property(nonatomic, weak) id<NewTabPageDelegate> ntpDelegate;

// The currently selected sorting for the Following feed.
@property(nonatomic, assign) FollowingFeedSortType followingFeedSortType;

// Whether the Following segment dot should currently be visible.
@property(nonatomic, assign) BOOL followingSegmentDotVisible;

// Initializes the header with the currently selected feed and the Following
// feed's sort type.
- (instancetype)initWithSelectedFeed:(FeedType)selectedFeed
               followingFeedSortType:
                   (FollowingFeedSortType)followingFeedSortType
          followingSegmentDotVisible:(BOOL)followingSegmentDotVisible
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Toggles the feed header's background blur. Animates the transition if
// |animated| is YES.
- (void)toggleBackgroundBlur:(BOOL)blurred animated:(BOOL)animated;

// Returns the height of the feed header.
- (CGFloat)feedHeaderHeight;

// Returns the height of the custom search engine view. Returns 0 if it is not
// visible.
- (CGFloat)customSearchEngineViewHeight;

// Updates the header view and re-applies constraints in response to the default
// search engine changing.
- (void)updateForDefaultSearchEngineChanged;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_
