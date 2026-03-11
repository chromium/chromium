// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_CONSTANTS_H_

#import <Foundation/Foundation.h>

/// The minimum height of the omnibox.
extern const CGFloat kOmniboxMinHeight;
/// The  margin  for the input plate container with its parent view.
extern const CGFloat kInputPlateMargin;
/// The corner radius for the input plate container.
extern const CGFloat kInputPlateCornerRadius;
/// The additional horizontal margin to ensure the composebox covers the top
/// omnibox.
extern const CGFloat kComposeboxOmniboxLayoutGuideHorizontalMargin;

// Accessibility identifier for the composebox.
extern NSString* const kComposeboxAccessibilityIdentifier;

// ==== Buttons in the composebox.
// Accessibility identifier for the plus button in the composebox.
extern NSString* const kComposeboxPlusButtonAccessibilityIdentifier;
// Accessibility identifier for the microphone button in the composebox.
extern NSString* const kComposeboxMicButtonAccessibilityIdentifier;
// Accessibility identifier for the Lens button in the composebox.
extern NSString* const kComposeboxLensButtonAccessibilityIdentifier;
// Accessibility identifier for the QR code button in the composebox.
extern NSString* const kComposeboxQRCodeButtonAccessibilityIdentifier;
// Accessibility identifier for the send button in the composebox.
extern NSString* const kComposeboxSendButtonAccessibilityIdentifier;
// Accessibility identifier for the AI mode button in the composebox.
extern NSString* const kComposeboxAIMButtonAccessibilityIdentifier;
// Accessibility identifier for the image generation button in the composebox.
extern NSString* const kComposeboxImageGenerationButtonAccessibilityIdentifier;

// ==== Actions in the plus menu.
// Accessibility identifier for the AI mode action in the plus menu.
extern NSString* const kComposeboxAIMActionAccessibilityIdentifier;
// Accessibility identifier for the create image action in the plus menu.
extern NSString* const kComposeboxImageGenerationActionAccessibilityIdentifier;
// Accessibility identifier for the Attach File button in the plus menu.
extern NSString* const kComposeboxAttachFileActionAccessibilityIdentifier;
// Accessibility identifier for the Gallery button in the plus menu.
extern NSString* const kComposeboxGalleryActionAccessibilityIdentifier;
// Accessibility identifier for the Camera button in the plus menu.
extern NSString* const kComposeboxCameraActionAccessibilityIdentifier;
// Accessibility identifier for the Attach Current Tab button in the plus menu.
extern NSString* const kComposeboxAttachCurrentTabActionAccessibilityIdentifier;
// Accessibility identifier for the Select Tabs button in the plus menu.
extern NSString* const kComposeboxSelectTabsActionAccessibilityIdentifier;

// Accessibility identifier for the tab picker's collection view.
extern NSString* const
    kComposeboxTabPickerCollectionViewAccessibilityIdentifier;
// Accessibility identifier for the empty state view in the tab picker.
extern NSString* const
    kComposeboxTabPickerEmptyStateViewAccessibilityIdentifier;

// Accessibility identifier for the carousel in the composebox.
extern NSString* const kComposeboxCarouselAccessibilityIdentifier;
// Accessibility identifier for an item in the carousel.
extern NSString* const kComposeboxCarouselItemAccessibilityIdentifier;
// Accessibility identifier for the close button of an item in the carousel.
extern NSString* const
    kComposeboxInputItemCellCloseButtonAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_CONSTANTS_H_
