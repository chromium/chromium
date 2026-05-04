// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_BEST_FEATURES_CAROUSEL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_BEST_FEATURES_CAROUSEL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class BestFeaturesItem;
@protocol PromoStyleViewControllerDelegate;

// A container view controller that manages a UIPageViewController to allow
// swiping between different feature promo screens.
@interface BestFeaturesCarouselViewController : UIViewController

// Delegate that handles screen dismissal.
@property(nonatomic, weak) id<PromoStyleViewControllerDelegate> delegate;

// Index of the currently presented screen. Used for accurate metrics logging.
@property(nonatomic, assign, readonly) int currentIndex;

// Initializes the carousel with the given items and starts at the specified
// index.
- (instancetype)initWithBestFeaturesItems:(NSArray<BestFeaturesItem*>*)items
                               startIndex:(int)startIndex
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_UI_BEST_FEATURES_CAROUSEL_VIEW_CONTROLLER_H_
