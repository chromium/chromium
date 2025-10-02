// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENTS_INTERACTOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENTS_INTERACTOR_H_

#include "base/ios/block_types.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet_detent_proxy.h"

// Facades either a system presentation or a custom Lens Overlay one, providing
// a common API between those two.
@interface LensOverlayBottomSheetDetentInteractor : NSObject

// Whether a system presentation is used.
@property(nonatomic, readonly) BOOL usesSystemPresentation;
// The selected detent identifier of the bottom sheet.
@property(nonatomic, readonly, copy)
    UISheetPresentationControllerDetentIdentifier selectedDetentIdentifier;

// Creates an instance based on a system bottom sheet presentation.
- (instancetype)initWithSystemSheetPresentationController:
    (UISheetPresentationController*)sheetPresentationController;
// Creates an instance based on a Lens Overlay bottom sheet presentation.
- (instancetype)initWithLensOverlayBottomSheet:
    (id<LensOverlayBottomSheet>)bottomSheet;

// Sets the detents to be used.
- (void)setDetents:(NSArray<LensOverlayBottomSheetDetentProxy*>*)detents;

// Sets the largest undimmed detent identifier.
// This is only used in the system presentation. When using the Lens Overlay
// bottom sheet, this method is no-op.
- (void)setLargestUndimmedDetentIdentifier:
    (UISheetPresentationControllerDetentIdentifier)
        largestUndimmedDetentIdentifier;

// Selects the given detent, optionally animated.
- (void)setSelectedDetentIdentifier:
            (UISheetPresentationControllerDetentIdentifier)
                selectedDetentIdentifier
                           animated:(BOOL)animated;

// Convenience init to create a detent proxy given an identifier and a static
// height. Internally it will choose the implementation based on the chosen
// presentation the interactor is scoped to.
- (LensOverlayBottomSheetDetentProxy*)
    detentWithIdentifier:
        (UISheetPresentationControllerDetentIdentifier)identifier
                  height:(CGFloat)height;

// Convenience init to create a detent proxy given an identifier and a dynamic
// height resolver. Internally it will choose the implementation based on the
// chosen presentation the interactor is scoped to.
- (LensOverlayBottomSheetDetentProxy*)
    detentWithIdentifier:
        (UISheetPresentationControllerDetentIdentifier)identifier
          heightResolver:(CGFloat (^)())heightResolver;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENTS_INTERACTOR_H_
