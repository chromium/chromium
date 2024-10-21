// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_bottom_sheet_presentation_delegate.h"

// Indicates the state of the bottom sheet
typedef NS_ENUM(NSUInteger, SheetDetentState) {
  // The bottom sheet is locked in large detent.
  SheetStateLockedInLargeDetent,
  // The bottom sheet is free to oscillate between medium and large.
  SheetStateUnrestrictedMovement,
  // The bottom sheet is presenting the consent dialog sheet.
  SheetStateConsentDialog,
  // The bottom sheet is in the peak state;
  SheetStatePeakEnabled,
};

// Manages the detents for a given bottom sheet.
@interface LensOverlayDetentsManager
    : NSObject <LensOverlayBottomSheetPresentationDelegate>

// Whether the sheet is in the largest detents.
@property(nonatomic, readonly) BOOL isInLargestDetent;

// Whether the sheet is in the peaking state.
@property(nonatomic, readonly) BOOL isPeaking;

// Creates a new detents manager scoped to the sheet instance.
- (instancetype)initWithBottomSheet:(UISheetPresentationController*)sheet;

// Adjust the detents of the given sheet based on the sheet state.
- (void)adjustDetentsForState:(SheetDetentState)state;

// Restricts the sheet to stay in the largest detent.
- (void)restrictSheetToLargeDetent:(BOOL)restrictToLargeDetent;

// Maximize the bottom sheet to the large detent.
- (void)requestMaximizeBottomSheet;

// Minimize the bottom sheet to the medium detent.
- (void)requestMinimizeBottomSheet;
@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_
