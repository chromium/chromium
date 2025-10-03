// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet.h"

// View controller for a custom sheet for Lens Overlay.
@interface LensOverlayBottomSheetViewController
    : UIViewController <LensOverlayBottomSheet>

// Sets the given content to be presented in the bottom sheet.
- (void)setContent:(UIViewController*)contentViewController;

// Presents the bottom sheet in view. Optionally animated.
- (void)presentAnimated:(BOOL)animated completion:(ProceduralBlock)completion;

// Dismissers the bottom sheet in view. Optionally animated.
- (void)dismissAnimated:(BOOL)animated completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_VIEW_CONTROLLER_H_
