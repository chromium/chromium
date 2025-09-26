// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_PRESENTATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_PRESENTATION_COMMANDS_H_

#import <Foundation/Foundation.h>

// Possible info messages to be shown in the bottom sheets.
enum class LensOverlayBottomSheetInfoMessageType {
  // Informs the user that there was an error detecting the text in the image
  kNoTranslatableTextWarning,
  // Informs the user that a translation was executed on the given image.
  kImageTranslatedIndication,
};

// Presentation commands for the bottom sheet.
// Bottom sheet content may request the container to be maximized or minimized,
// e.g. when the user selects a result that opens an image viewer.
@protocol LensOverlayBottomSheetPresentationCommands <NSObject>

// Request resizing the bottom sheet to maximum size.
- (void)requestMaximizeBottomSheet;

// Request resizing the bottom sheet to minimum size.
- (void)requestMinimizeBottomSheet;

// Handle a selection result loaded in the bottom sheet.
- (void)adjustForSelectionResult;

// Handle a translation result loaded in the bottom sheet.
- (void)adjustForTranslateResult;

// Hides the bottom sheet without destroying the presentation.
- (void)hideBottomSheetWithCompletion:(void (^)(void))completion;

// Hides the results UI and shows the given informational message.
- (void)showInfoMessage:(LensOverlayBottomSheetInfoMessageType)infoMessageType;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_BOTTOM_SHEET_PRESENTATION_COMMANDS_H_
