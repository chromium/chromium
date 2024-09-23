// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/discover_feed/model/feed_constants.h"

@protocol FeedControlDelegate;
@protocol FeedMenuCommands;
@class FeedMetricsRecorder;
@protocol NewTabPageDelegate;

@interface FeedHeaderViewController : UIViewController

// Button for opening top-level feed management menu.
@property(nonatomic, readonly, strong) UIButton* managementButton;

// Delegate for controlling the presented feed.
@property(nonatomic, weak) id<FeedControlDelegate> feedControlDelegate;

// Delegate to communicate back to the New Tab Page coordinator.
@property(nonatomic, weak) id<NewTabPageDelegate> NTPDelegate;

// The currently selected sorting for the Following feed.
@property(nonatomic, assign) FollowingFeedSortType followingFeedSortType;

// Feed metrics recorder.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

// Object that can open the feed menu.
@property(nonatomic, weak) id<FeedMenuCommands> feedMenuHandler;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns the height of the feed header.
- (CGFloat)feedHeaderHeight;

// Returns the height of the custom search engine view. Returns 0 if it is not
// visible.
- (CGFloat)customSearchEngineViewHeight;

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

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_HEADER_VIEW_CONTROLLER_H_
