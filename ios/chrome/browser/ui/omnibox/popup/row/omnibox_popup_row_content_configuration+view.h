// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_VIEW_H_

#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_configuration.h"

@protocol OmniboxIcon;

/// OmniboxPopupRow content configuration interface for the content view.
@interface OmniboxPopupRowContentConfiguration ()

// Background.
@property(nonatomic, assign, readonly) BOOL showSelectedBackgroundView;

// Leading Icon.
@property(nonatomic, strong, readonly) id<OmniboxIcon> leadingIcon;
@property(nonatomic, assign, readonly) BOOL leadingIconHighlighted;

// Primary text.
@property(nonatomic, strong, readonly) NSAttributedString* primaryText;
@property(nonatomic, assign, readonly) NSInteger primaryTextNumberOfLines;

// Secondary Text.
@property(nonatomic, strong, readonly) NSAttributedString* secondaryText;
@property(nonatomic, assign, readonly) NSInteger secondaryTextNumberOfLines;
@property(nonatomic, assign, readonly) BOOL secondaryTextFading;
@property(nonatomic, assign, readonly) BOOL secondaryTextDisplayAsURL;

// Trailing Icon.
@property(nonatomic, strong, readonly) UIImage* trailingIcon;
@property(nonatomic, strong, readonly) UIColor* trailingIconTintColor;
@property(nonatomic, strong, readonly)
    NSString* trailingButtonAccessibilityIdentifier;

// Margins.
@property(nonatomic, assign, readonly)
    NSDirectionalEdgeInsets directionalLayoutMargin;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_VIEW_H_
