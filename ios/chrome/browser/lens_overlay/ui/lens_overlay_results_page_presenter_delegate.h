// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_sheet_detent_state.h"

@class LensOverlayResultsPagePresenter;

// The methods adopted by the object you use to manage user interactions with
// the Lens result page.
@protocol LensOverlayResultsPagePresenterDelegate <NSObject>

// Informs the delegate that a user swipe has caused the bottom sheet to cross
// the close threshold, resulting in its dismissal.
- (void)lensOverlayResultsPagePresenterWillInitiateGestureDrivenDismiss:
    (LensOverlayResultsPagePresenter*)presenter;

// Tells the delegate that the results bottom sheet detent dimension has
// changed.
- (void)lensOverlayResultsPagePresenter:
            (LensOverlayResultsPagePresenter*)presenter
                didUpdateDimensionState:(SheetDimensionState)state;

// Asks the delegate to update the vertical occlusion offset to the given value.
- (void)lensOverlayResultsPagePresenter:
            (LensOverlayResultsPagePresenter*)presenter
          updateVerticalOcclusionOffset:(CGFloat)offsetNeeded;

// Tells the delegate that the layout guide for the visible area was adjusted.
- (void)lensOverlayResultsPagePresenter:
            (LensOverlayResultsPagePresenter*)presenter
        didAdjustVisibleAreaLayoutGuide:(UILayoutGuide*)visibleAreaLayoutGuide;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_
