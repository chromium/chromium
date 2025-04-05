// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_sheet_detent_state.h"

@protocol LensOverlayDetentsManagerDelegate;

// Manages the detents for a given bottom sheet, adapting to different detent
// sizes.
//
// The sheet detent state defines the set of detents the sheet can settle
// into once the user completes a manual drag gesture and releases it.
// The current presentation strategy dictates the possible height variations
// between the available detents of each state.
//
// While the semantic meaning of each state is consistent, the way each state
// presentation is dictated by the current presentation strategy. The employed
// strategy dictates variation in height of detents.
//
// The number of detents can be subject to change and its consistency is not
// guaranteed between presentation strategies.
@interface LensOverlayDetentsManager : NSObject

// The estimated detent medium detent height, with respect to the current
// presentation strategy.
@property(nonatomic, readonly) CGFloat estimatedMediumDetentHeight;

// The object notified of bottom sheet detent changes.
@property(nonatomic, weak) id<LensOverlayDetentsManagerDelegate> delegate;

// Current sheet dimension.
@property(nonatomic, readonly) SheetDimensionState sheetDimension;

// The height of the info message in points.
@property(nonatomic, assign) CGFloat infoMessageHeight;

// The strategy to use when presenting.
//
// Changing the presentation strategy adjusts the detents for unrestricted
// movement.
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

// Reacts to changes in detents and dimension states.
@protocol LensOverlayDetentsManagerDelegate <NSObject>

// Called when the dimension state changes.
- (void)lensOverlayDetentsManagerDidChangeDimensionState:
    (LensOverlayDetentsManager*)detentsManager;

// Asks the delegate for permission to dismiss the presentation.
- (BOOL)lensOverlayDetentsManagerShouldDismissBottomSheet:
    (LensOverlayDetentsManager*)detentsManager;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_DETENTS_MANAGER_H_
