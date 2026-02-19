// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_ICON_DETAIL_VIEW_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_ICON_DETAIL_VIEW_CONFIG_H_

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"

// The possible layout types for a given `IconDetailView`. These values
// determine how the content within the `IconDetailView` is arranged.
enum class IconDetailViewLayoutType {
  // A prominent layout with a larger icon and more spacing between
  // the title and description. Suitable for single-row displays.
  kHero = 1,
  // A more condensed layout with a smaller icon and less spacing
  // between the title and description. Useful for multi-row views.
  kCompact = 2,
  kMaxValue = kCompact
};

// The shape of the badge displayed on the icon.
enum class IconDetailViewBadgeShape {
  // A square-shaped badge.
  kSquare,
  // A circle-shaped badge.
  kCircle,
  kMaxValue = kCircle
};

// Configuration for the shape of the badge displayed on the icon.
struct BadgeShapeConfig {
  // Shape of the badge.
  IconDetailViewBadgeShape shape;
  // Size of the badge.
  CGFloat size;
  // Radius of the top left corner.
  CGFloat topLeftRadius;
  // Radius of the top right corner.
  CGFloat topRightRadius;
  // Radius of the bottom left corner.
  CGFloat bottomLeftRadius;
  // Radius of the bottom right corner.
  CGFloat bottomRightRadius;
};

@class IconViewConfiguration;
enum class IconViewSourceType;
@class NewTabPageColorPalette;

// Configuration for the `IconDetailView`.
@interface IconDetailViewConfig : MagicStackModule

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)
// Content properties.
// The title to be displayed in the view.
@property(nonatomic, copy) NSString* titleText;
// The descriptive text to be displayed in the view.
@property(nonatomic, copy) NSString* descriptionText;
// The item layout type. This determines the spacing of elements within the
// view.
@property(nonatomic, assign) IconDetailViewLayoutType layoutType;
// The accessibility identifier for the view.
@property(nonatomic, copy) NSString* accessibilityIdentifier;

// Checkmark properties.
// Whether or not the icon should be displayed with a green checkmark to
// indicate a completed state.
@property(nonatomic, assign) BOOL showCheckmark;

// Background image properties.
// The image used to create the background image for the icon. If valid, this
// will be used instead of the symbol image.
@property(nonatomic, strong) UIImage* backgroundImage;

// Icon properties.
// The name of the image or symbol to be displayed in the view.
@property(nonatomic, copy) NSString* iconName;
// The type of resource (symbol, local image) to retrieve this icon from.
@property(nonatomic, assign) IconViewSourceType iconSource;
// The color palette of the symbol displayed in the view.
@property(nonatomic, copy) NSArray<UIColor*>* symbolColorPalette;
// The background color of the symbol displayed in the view.
@property(nonatomic, strong) UIColor* symbolBackgroundColor;
// Indicates whether the symbol is a default symbol.
@property(nonatomic, assign) BOOL usesDefaultSymbol;
// The width of the icon.
@property(nonatomic, assign) CGFloat iconWidth;

// Badge properties.
// The symbol name of the Badge Icon to be displayed in the view.
@property(nonatomic, copy) NSString* badgeSymbolName;
// The color palette of the badge symbol displayed in the view.
@property(nonatomic, copy) NSArray<UIColor*>* badgeColorPalette;
// The background color of the Badge Icon to be displayed in the view.
@property(nonatomic, strong) UIColor* badgeBackgroundColor;
// The shape configuration of the badge displayed on the icon.
@property(nonatomic, assign) BadgeShapeConfig badgeShapeConfig;
// Indicates whether the Badge's Icon is a default symbol.
@property(nonatomic, assign) BOOL badgeUsesDefaultSymbol;

@property(nonatomic, strong) NewTabPageColorPalette* ntpBackgroundColorPalette;
// LINT.ThenChange(icon_detail_view_config.mm:Copy)

// Returns the configuration for an `IconView` associated with this
// `IconDetailView`.
- (IconViewConfiguration*)iconViewConfiguration:(BOOL)inSquare;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_ICON_DETAIL_VIEW_CONFIG_H_
