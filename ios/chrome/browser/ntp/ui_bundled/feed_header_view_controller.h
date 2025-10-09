// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/discover_feed/model/feed_constants.h"

@protocol FeedControlDelegate;
@class FeedMetricsRecorder;
@protocol NewTabPageDelegate;

@interface FeedHeaderViewController : UIViewController

// Delegate for controlling the presented feed.
@property(nonatomic, weak) id<FeedControlDelegate> feedControlDelegate;

// Delegate to communicate back to the New Tab Page coordinator.
@property(nonatomic, weak) id<NewTabPageDelegate> NTPDelegate;

// Feed metrics recorder.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns the height of the feed header.
- (CGFloat)feedHeaderHeight;

// Updates the header view and re-applies constraints in response to the default
// search engine changing.
- (void)updateForDefaultSearchEngineChanged;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_HEADER_VIEW_CONTROLLER_H_
