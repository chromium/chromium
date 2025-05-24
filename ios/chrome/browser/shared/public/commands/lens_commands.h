// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_COMMANDS_H_

@class OpenLensInputSelectionCommand;
@class SearchImageWithLensCommand;
enum class LensEntrypoint;

// Different causes of dismising the overlay.
typedef NS_ENUM(NSUInteger, LensOverlayDismissalCause) {
  // The user dismissed the overlay with a swipe down from selection.
  LensOverlayDismissalCauseSwipeDownFromSelection,
  // The user dismissed the overlay with a swipe down from translate.
  LensOverlayDismissalCauseSwipeDownFromTranslate,
  // The user dismissed the overlay by pressing the dismiss button.
  LensOverlayDismissalCauseDismissButton,
  // An external navigation caused the overlay to be dismissed.
  LensOverlayDismissalCauseExternalNavigation
};

// Commands related to Lens.
@protocol LensCommands

// Search for an image with Lens, using `command` parameters.
- (void)searchImageWithLens:(SearchImageWithLensCommand*)command;

// Opens the input selection UI with the given settings.
- (void)openLensInputSelection:(OpenLensInputSelectionCommand*)command;

// Notifies that the associated post capture will be dismissed.
- (void)lensOverlayWillDismissWithCause:
    (LensOverlayDismissalCause)dismissalCause;

// Notifies that the associated post capture has been dismissed.
- (void)lensOverlayDidDismissWithCause:
    (LensOverlayDismissalCause)dismissalCause;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_COMMANDS_H_
