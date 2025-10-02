// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENT_PROXY_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENT_PROXY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet_detent.h"

// Internal class used as a proxy encompassing the common behavior of the system
// detents and the Lens Overlay ones.
@interface LensOverlayBottomSheetDetentProxy : NSObject

// The UIKit detent this instance proxies. `nil` if not present.
@property(nonatomic, readonly)
    UISheetPresentationControllerDetent* systemDetent;

// The Lens Overlay detent this instance proxies. `nil` if not present.
@property(nonatomic, readonly) LensOverlayBottomSheetDetent* lensOverlayDetent;

// The identifier of this detent.
@property(nonatomic, readonly, copy) NSString* identifier;

// Creates a detent proxy for a UIKit detent.
- (instancetype)initWithSystemDetent:
    (UISheetPresentationControllerDetent*)systemDetent;

// Creates a detent proxy for a Lens Overlay custom detent.
- (instancetype)initWithLensOverlayDetent:
    (LensOverlayBottomSheetDetent*)lensOverlayDetent;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENT_PROXY_H_
