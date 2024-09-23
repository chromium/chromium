// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestion;
@protocol FaviconRetriever;
@protocol ImageRetriever;
@protocol OmniboxIcon;
@protocol OmniboxPopupRowDelegate;
@protocol OmniboxPopupActionsRowDelegate;

extern NSString* const OmniboxPopupRowCellReuseIdentifier;
/// This minimum height causes most of the rows to be the same height. Some have
/// multiline answers, so those heights may be taller than this minimum.
extern const CGFloat kOmniboxPopupCellMinimumHeight;

/// Content configuration of the omnibox popup row, contains the logic of the
/// row UI.
@interface OmniboxPopupRowContentConfiguration
    : NSObject <UIContentConfiguration>

/// Autocomplete suggestion.
@property(nonatomic, strong) id<AutocompleteSuggestion> suggestion;
/// Delegate for events in OmniboxPopupRow.
@property(nonatomic, weak)
    id<OmniboxPopupRowDelegate, OmniboxPopupActionsRowDelegate>
        delegate;
/// Index path of the row.
@property(nonatomic, strong) NSIndexPath* indexPath;
/// Whether the bottom cell separator should be shown.
@property(nonatomic, assign) BOOL showSeparator;
/// Forced semantic content attribute.
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;
/// Favicon retriever for `OmniboxIconView`.
@property(nonatomic, weak) id<FaviconRetriever> faviconRetriever;
/// Image retriever for `OmniboxIconView`.
@property(nonatomic, weak) id<ImageRetriever> imageRetriever;

/// Returns the default configuration for a list cell.
+ (instancetype)cellConfiguration;

/// Returns the input string but painted white when the blue and white
/// highlighting is enabled in pedals. Returns the original string otherwise.
+ (NSAttributedString*)highlightedAttributedStringWithString:
    (NSAttributedString*)string;

- (instancetype)init NS_UNAVAILABLE;

#pragma mark - Content View interface

// Background.
@property(nonatomic, assign, readonly) BOOL isBackgroundHighlighted;

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
// Some margins are updated with popout omnibox.
@property(nonatomic, assign, readonly) BOOL isPopoutOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_CONFIGURATION_H_
