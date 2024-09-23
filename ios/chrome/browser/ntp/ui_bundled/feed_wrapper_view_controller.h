// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_WRAPPER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_WRAPPER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Delegate for FeedWrapperViewController.
@protocol FeedWrapperViewControllerDelegate <NSObject>

// Called when the theme for feed should be synchronized with the system theme
// (light/dark).
- (void)updateTheme;

@end

// View controller wrapping feed view controller
// (`self.feedViewController`) originating from the provider.
@interface FeedWrapperViewController : UIViewController

// Feed view controller being contained by this view controller. This is the
// view controller that is wrapped by this view controller.
@property(nonatomic, strong, readonly) UIViewController* feedViewController;

// The containing collection view of the NTP. Can either be the feed if the feed
// is visible, or an empty collection view if not.
@property(nonatomic, weak) UICollectionView* contentCollectionView;

// Initializes view controller with a feed view controller originating from the
// feed service.
- (instancetype)initWithDelegate:(id<FeedWrapperViewControllerDelegate>)delegate
              feedViewController:(UIViewController*)feedViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns the index of the last visible feed card.
- (NSUInteger)lastVisibleFeedCardIndex;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_WRAPPER_VIEW_CONTROLLER_H_
