// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSTANTS_H_

#import <UIKit/UIKit.h>

extern const CGFloat kOmniboxPlaceholderAlpha;

extern NSString* const kOmniboxLeadingImageDefaultAccessibilityIdentifier;

extern NSString* const kOmniboxLeadingImageEmptyTextAccessibilityIdentifier;

extern NSString* const
    kOmniboxLeadingImageSuggestionImageAccessibilityIdentifier;

// Size of the leading image view.
extern const CGFloat kOmniboxLeadingImageSize;

// Offset from the leading edge to the image view (used when the image is
// shown).
extern const CGFloat kOmniboxLeadingImageViewEdgeOffset;

// Offset from the leading edge to the textfield when no image is shown.
extern const CGFloat kOmniboxTextFieldLeadingOffsetNoImage;

// Space between the leading button and the textfield when a button is shown.
extern const CGFloat kOmniboxTextFieldLeadingOffsetImage;

// The offset from the leading edge to the textfield when an image is shown.
// This is a sum of kOmniboxLeadingImageViewEdgeOffset,
// kOmniboxLeadingImageSize, and kOmniboxTextFieldLeadingOffsetImage.
extern const CGFloat kOmniboxEditOffset;

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSTANTS_H_
