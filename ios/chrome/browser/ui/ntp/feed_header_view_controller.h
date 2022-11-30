// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/discover_feed/feed_constants.h"

@protocol FeedControlDelegate;
@class FeedMetricsRecorder;
@protocol NewTabPageDelegate;

@interface FeedHeaderViewController : UIViewController

// Button for opening top-level feed menu.
@property(nonatomic, readonly, strong) UIButton* menuButton;

// Delegate for controlling the presented feed.
@property(nonatomic, weak) id<FeedControlDelegate> feedControlDelegate;

// Delegate to communicate back to the New Tab Page coordinator.
@property(nonatomic, weak) id<NewTabPageDelegate> ntpDelegate;

// The currently selected sorting for the Following feed.
@property(nonatomic, assign) FollowingFeedSortType followingFeedSortType;

// Feed metrics recorder.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

// Initializes the header with the currently selected feed and the Following
// feed's sort type.
- (instancetype)initWithFollowingFeedSortType:
                    (FollowingFeedSortType)followingFeedSortType
                   followingSegmentDotVisible:(BOOL)followingSegmentDotVisible
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Toggles the feed header's background blur. Animates the transition if
// `animated` is YES.
- (void)toggleBackgroundBlur:(BOOL)blurred animated:(BOOL)animated;

// Returns the height of the feed header.
- (CGFloat)feedHeaderHeight;

// Returns the height of the custom search engine view. Returns 0 if it is not
// visible.
- (CGFloat)customSearchEngineViewHeight;

// Updates the unseen content dot in the Following segment. Will only show the
// dot if there is unseen content and if the user is not currently on the
// Following feed.
- (void)updateFollowingSegmentDotForUnseenContent:(BOOL)hasUnseenContent;

// Updates the header view and re-applies constraints in response to the default
// search engine changing.
- (void)updateForDefaultSearchEngineChanged;

// Updates the header for when the user turns the feed off from the header menu.
- (void)updateForFeedVisibilityChanged;

// Updates the header for when the Following Feed visibility has changed.
- (void)updateForFollowingFeedVisibilityChanged;

// Updates the segmented control and sort button for the current feed type.
- (void)updateForSelectedFeed;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_
