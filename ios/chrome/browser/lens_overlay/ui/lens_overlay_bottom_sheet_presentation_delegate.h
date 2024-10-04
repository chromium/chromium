// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_PRESENTATION_DELEGATE_H_

// Presentation delegate for the bottom sheet.
// Bottom sheet content may request the container to be maximized or minimized,
// e.g. when the user selects a result that opens an image viewer.
@protocol LensOverlayBottomSheetPresentationDelegate <NSObject>

// Request resizing the bottom sheet to maximum size.
- (void)requestMaximizeBottomSheet;

// Request resizing the bottom sheet to minimum size.
- (void)requestMinimizeBottomSheet;

// Whether to restrict sheet to stay into the large detent.
- (void)restrictSheetToLargeDetent:(BOOL)restrictToLargeDetent;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_PRESENTATION_DELEGATE_H_
