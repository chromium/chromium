// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_sheet_detent_state.h"

// Delegate for the results page presenter.
@protocol LensOverlayResultsPagePresenterDelegate

// The close height threshold was reached and the bottom sheet will close.
- (void)onResultsPageWillInitiateGestureDrivenDismiss;

// The results bottom sheet detent dimension has changed.
- (void)onResultsPageDimensionStateChanged:(SheetDimensionState)state;

// The occlusion insets amount has been determined.
- (void)onResultsPageVerticalOcclusionInsetsSettled:(CGFloat)offsetNeeded;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTER_DELEGATE_H_
