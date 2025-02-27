// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_sheet_detent_state.h"

@protocol LensOverlayDetentsChangeObserver;

// Manages the detents for a given bottom sheet, adapting to different detent
// sizes.
@interface LensOverlayDetentsManager : NSObject

// The estimated detent medium detent height, with respect to the current
// presentation strategy.
@property(nonatomic, readonly) CGFloat estimatedMediumDetentHeight;

// The object notified of bottom sheet detent changes.
@property(nonatomic, weak) id<LensOverlayDetentsChangeObserver> observer;

// Current sheet dimension.
@property(nonatomic, readonly) SheetDimensionState sheetDimension;

// The strategy to use when presenting in unrestricted mode.
@property(nonatomic, assign)
    SheetDetentPresentationStategy presentationStrategy;

// Creates a new detents manager scoped to the sheet instance.
// Starts by default in 'selection' mode.
- (instancetype)initWithBottomSheet:(UISheetPresentationController*)sheet
                             window:(UIWindow*)window;

// Creates a new detents manager scoped to the sheet instance, starting
// initially in the given presentation strategy.
- (instancetype)initWithBottomSheet:(UISheetPresentationController*)sheet
                             window:(UIWindow*)window
               presentationStrategy:
                   (SheetDetentPresentationStategy)presentationStrategy
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adjusts the detents of the given sheet based on the sheet state.
- (void)adjustDetentsForState:(SheetDetentState)state;

// Maximizes the bottom sheet to the large detent.
- (void)requestMaximizeBottomSheet;

// Minimize the bottom sheet to the medium detent.
- (void)requestMinimizeBottomSheet;

@end

// Observes changes in the detents and dimension states.
@protocol LensOverlayDetentsChangeObserver <NSObject>

// Called when the dimension state changes. Does not report the initial value,
// only publishes changes recorded after the subscription.
- (void)onBottomSheetDimensionStateChanged:(SheetDimensionState)state;

// Called before dismissing the bottom sheet.
- (BOOL)bottomSheetShouldDismissFromState:(SheetDimensionState)state;
@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_
