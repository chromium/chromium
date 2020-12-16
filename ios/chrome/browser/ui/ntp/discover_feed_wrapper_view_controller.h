// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_WRAPPER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_WRAPPER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View controller wrapping a Discover feed view controller
// (|self.discoverFeed|) originating from the provider.
@interface DiscoverFeedWrapperViewController : UIViewController

// Feed view controller being contained by this view controller. This is the
// view controller that is wrapped by this view controller.
@property(nonatomic, strong, readonly) UIViewController* discoverFeed;

// Collection view that contains the feed articles. Is a subview of
// |self.discoverFeed|.
@property(nonatomic, weak, readonly) UICollectionView* feedCollectionView;

// Initializes view controller with the Discover feed view controller
// originating from the Discover feed provider.
- (instancetype)initWithDiscoverFeedViewController:
    (UIViewController*)discoverFeed NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_WRAPPER_VIEW_CONTROLLER_H_
