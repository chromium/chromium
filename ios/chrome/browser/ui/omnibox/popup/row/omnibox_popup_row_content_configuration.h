// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_cell.h"

@protocol AutocompleteSuggestion;
@protocol FaviconRetriever;
@protocol ImageRetriever;
@protocol OmniboxIcon;
@protocol OmniboxPopupRowDelegate;

/// Content configuration of the omnibox popup row, contains the logic of the
/// row UI.
@interface OmniboxPopupRowContentConfiguration
    : NSObject <UIContentConfiguration>

/// Autocomplete suggestion.
@property(nonatomic, strong) id<AutocompleteSuggestion> suggestion;
/// Delegate for events in OmniboxPopupRow.
@property(nonatomic, weak) id<OmniboxPopupRowDelegate> delegate;
/// Index path of the row.
@property(nonatomic, strong) NSIndexPath* indexPath;
/// Whether the bottom cell separator should be shown.
@property(nonatomic, assign) BOOL showSeparator;
/// Forced semantic content attribute.
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;
/// Omnibox textfield layout guide from  `OmniboxPopupViewController`.
@property(nonatomic, weak) UILayoutGuide* omniboxLayoutGuide;
/// Favicon retriever for `OmniboxIconView`.
@property(nonatomic, weak) id<FaviconRetriever> faviconRetriever;
/// Image retriever for `OmniboxIconView`.
@property(nonatomic, weak) id<ImageRetriever> imageRetriever;

/// Returns the default configuration for a list cell.
+ (instancetype)cellConfiguration;

- (instancetype)init NS_UNAVAILABLE;

#pragma mark - Content View interface

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

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_H_
