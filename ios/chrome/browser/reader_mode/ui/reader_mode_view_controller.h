// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/reader_mode/ui/reader_mode_consumer.h"

@class ReaderModeViewController;
@protocol ReaderModeMutator;
@protocol OverscrollActionsControllerDelegate;

// Delegate for ReaderModeViewController.
@protocol ReaderModeViewControllerDelegate <NSObject>
// Called when the tabs closure animation completes.
- (void)readerModeViewControllerAnimationDidComplete:
    (ReaderModeViewController*)controller;
@end

// View controller for displaying the Reader mode content.
@interface ReaderModeViewController : UIViewController <ReaderModeConsumer>

@property(nonatomic, weak) id<ReaderModeViewControllerDelegate> delegate;
@property(nonatomic, weak) id<ReaderModeMutator> mutator;
// Required to support Overscroll Actions UI, which is displayed when Reader
// mode is pulled down.
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollDelegate;

// Adds `self` as child view controller of `parent` and does the appropriate
// calls to `willMoveToParentViewController:` and
// `didMoveToParentViewController:`. If `animated` then an animation will be
// used to add the view.
- (void)moveToParentViewController:(UIViewController*)parent
                          animated:(BOOL)animated;
// Removes `self` as child view controller of `parent` and does the appropriate
// calls to `willMoveToParentViewController:` and
// `didMoveToParentViewController:`. If `animated` then an animation will be
// used to remove the view.
- (void)removeFromParentViewControllerAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_VIEW_CONTROLLER_H_
