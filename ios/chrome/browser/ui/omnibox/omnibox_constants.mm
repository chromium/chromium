// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"

const CGFloat kOmniboxPlaceholderAlpha = 0.3;

NSString* const kOmniboxLeadingImageDefaultAccessibilityIdentifier =
    @"OmniboxLeadingImageDefaultAccessibilityIdentifier";

NSString* const kOmniboxLeadingImageEmptyTextAccessibilityIdentifier =
    @"OmniboxLeadingImageEmptyTextAccessibilityIdentifier";

NSString* const kOmniboxLeadingImageSuggestionImageAccessibilityIdentifier =
    @"OmniboxLeadingImageSuggestionImageAccessibilityIdentifier";

constexpr CGFloat kOmniboxLeadingImageSize = 30;
constexpr CGFloat kOmniboxLeadingImageViewEdgeOffset = 7;
constexpr CGFloat kOmniboxTextFieldLeadingOffsetNoImage = 16;
constexpr CGFloat kOmniboxTextFieldLeadingOffsetImage = 14;
constexpr CGFloat kOmniboxEditOffset = kOmniboxLeadingImageViewEdgeOffset +
                                       kOmniboxLeadingImageSize +
                                       kOmniboxTextFieldLeadingOffsetImage;
