// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTING_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTING_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_sheet_detent_state.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_bottom_sheet_presentation_commands.h"

@protocol LensOverlayResultsPagePresenterDelegate;

// Protocol for presenting the Lens results bottom sheet.
@protocol LensOverlayResultsPagePresenting <
    LensOverlayBottomSheetPresentationCommands>

// Whether the results page is currently presented.
@property(nonatomic, assign, readonly) BOOL isResultPageVisible;

// Current sheet dimension.
@property(nonatomic, readonly) SheetDimensionState sheetDimension;

// Delegate for the presenter events.
@property(nonatomic, weak) id<LensOverlayResultsPagePresenterDelegate> delegate;

// The current height of the results page.
@property(nonatomic, readonly) CGFloat presentedResultsPageHeight;

// Presents the result page over the base view controller.
- (void)presentResultsPageAnimated:(BOOL)animated
                     maximizeSheet:(BOOL)maximizeSheet
                  startInTranslate:(BOOL)startInTranslate
                        completion:(void (^)(void))completion;

// Readjusts the presentation if there was a change in window dimensions.
- (void)readjustPresentationIfNeeded;

// Dismisses the presented page from the base view controller.
- (void)dismissResultsPageAnimated:(BOOL)animated
                        completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULTS_PAGE_PRESENTING_H_
