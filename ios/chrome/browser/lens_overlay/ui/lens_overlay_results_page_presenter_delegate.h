// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_sheet_detent_state.h"

@protocol LensOverlayResultsPagePresenting;

// The methods adopted by the object you use to manage user interactions with
// the Lens result page.
@protocol LensOverlayResultsPagePresenterDelegate <NSObject>

// Informs the delegate that a user swipe has caused the bottom sheet to cross
// the close threshold, resulting in its dismissal.
- (void)lensOverlayResultsPagePresenterWillInitiateGestureDrivenDismiss:
    (id<LensOverlayResultsPagePresenting>)presenter;

// Tells the delegate that the results bottom sheet detent dimension has
// changed.
- (void)lensOverlayResultsPagePresenter:
            (id<LensOverlayResultsPagePresenting>)presenter
                didUpdateDimensionState:(SheetDimensionState)state;

// Notifies the delegate that the side panel is shown.
- (void)lensOverlayResultsPagePresenter:
            (id<LensOverlayResultsPagePresenting>)presenter
        updateHorizontalOcclusionOffset:(CGFloat)horizontalOffset;

// Asks the delegate to update the vertical occlusion offset to the given value.
- (void)lensOverlayResultsPagePresenter:
            (id<LensOverlayResultsPagePresenting>)presenter
          updateVerticalOcclusionOffset:(CGFloat)offsetNeeded;

// Tells the delegate that the layout guide for the visible area was adjusted.
- (void)lensOverlayResultsPagePresenter:
            (id<LensOverlayResultsPagePresenting>)presenter
        didAdjustVisibleAreaLayoutGuide:(UILayoutGuide*)visibleAreaLayoutGuide;

// Offers the dependent UI a chance to gracefully exit before the bottom sheet
// dismisses completely.
- (void)lensOverlayResultsPagePresenter:
            (id<LensOverlayResultsPagePresenting>)presenter
    animateAttachedUIDismissWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_
