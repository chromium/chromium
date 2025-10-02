// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet_detent.h"

@protocol LensOverlayBottomSheetDetentsDelegate;
@protocol LensOverlayBottomSheetPresenterDelegate;

// Defines the capabilities of a custom bottom sheet.
@protocol LensOverlayBottomSheet <NSObject>

// The delegate for events related to the bottom sheet detents.
@property(nonatomic, weak) id<LensOverlayBottomSheetDetentsDelegate>
    detentsDelegate;

// The delegate for events related to the bottom sheet presentation.
@property(nonatomic, weak) id<LensOverlayBottomSheetPresenterDelegate>
    sheetDelegate;

// The list of available detents.
@property(nonatomic, copy) NSArray<LensOverlayBottomSheetDetent*>* detents;

// The identifier for the selected detent.
@property(nonatomic, readonly, copy) NSString* selectedDetentIdentifier;

// Whether the bottom sheet is currently presented.
@property(nonatomic, readonly, getter=isBottomSheetPresented)
    BOOL bottomSheetPresented;

// Sets the selected detent identifier, optionally animated.
- (void)setSelectedDetentIdentifier:(NSString*)selectedDetentIdentifier
                           animated:(BOOL)animated;

@end

// Delegate informed of detents related events for a
// `LensOverlayBottomSheetPresenter`.
@protocol LensOverlayBottomSheetDetentsDelegate

// Called when the selected detent identifier is changed.
- (void)lensOverlayBottomSheetDidChangeSelectedDetentIdentifier:
    (id<LensOverlayBottomSheet>)bottomSheet;

// Asks the delegate if the presentation controller should dismiss in response
// to user action.
- (BOOL)lensOverlayBottomSheetShouldDismiss:
    (id<LensOverlayBottomSheet>)bottomSheet;

@end

// Delegate informed of presentation lifecycle events for a
// `LensOverlayBottomSheetPresenter`.
@protocol LensOverlayBottomSheetPresenterDelegate

// Called before the bottom sheet was dismissed.
- (void)lensOverlayBottomSheetWillDismiss:
    (id<LensOverlayBottomSheet>)bottomSheet;

// Called after the bottom sheet was dismissed.
- (void)lensOverlayBottomSheetDidDismiss:
    (id<LensOverlayBottomSheet>)bottomSheet;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_H_
