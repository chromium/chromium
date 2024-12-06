// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_sheet_detent_state.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_bottom_sheet_presentation_delegate.h"

@protocol LensOverlayResultsPagePresenterDelegate;
@class LensResultPageViewController;
@class SceneState;

// Presenter for the Lens results bottom sheet.
@interface LensOverlayResultsPagePresenter
    : NSObject <LensOverlayBottomSheetPresentationDelegate>

// Whether the results page is currently presented.
@property(nonatomic, assign, readonly) BOOL isResultPageVisible;

// Current sheet dimension.
@property(nonatomic, readonly) SheetDimensionState sheetDimension;

// Delegate for the presenter events.
@property(nonatomic, weak) id<LensOverlayResultsPagePresenterDelegate> delegate;

// Creates a new instance of the presenter.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                  resultPageViewController:
                      (LensResultPageViewController*)resultViewController;

// Presents the result page over the base view controller.
- (void)presentResultsPageAnimated:(BOOL)animated
                        sceneState:(SceneState*)sceneState
                     maximizeSheet:(BOOL)maximizeSheet
                        completion:(void (^)(void))completion;

// Dismisses the presented page from the base view controller.
- (void)dismissResultsPageAnimated:(BOOL)animated
                        completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_H_
