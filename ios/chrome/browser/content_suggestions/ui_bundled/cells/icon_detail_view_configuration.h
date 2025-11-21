// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_DETAIL_VIEW_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_DETAIL_VIEW_CONFIGURATION_H_

#import <UIKit/UIKit.h>

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

// Configuration for the `IconDetailView`.
@interface IconDetailViewConfiguration : NSObject

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

// Symbol properties.
// The symbol to be displayed in the view.
@property(nonatomic, copy) NSString* symbolName;
// The color palette of the symbol displayed in the view.
@property(nonatomic, copy) NSArray<UIColor*>* symbolColorPalette;
// The background color of the symbol displayed in the view.
@property(nonatomic, strong) UIColor* symbolBackgroundColor;
// The background color of the icon container in the view.
@property(nonatomic, strong) UIColor* symbolContainerBackgroundColor;
// Indicates whether the symbol is a default symbol.
@property(nonatomic, assign) BOOL usesDefaultSymbol;
// The width of the symbol.
@property(nonatomic, assign) CGFloat symbolWidth;

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

// Convenience initializer to create a configuration with `titleText` and
// `descriptionText`.
+ (instancetype)configurationWithTitleText:(NSString*)titleText
                           descriptionText:(NSString*)descriptionText;

// Convenience initializer to create a configuration with `titleText`,
// `descriptionText`, `layoutType`, and `badgeShape`.
+ (instancetype)configurationWithTitleText:(NSString*)titleText
                           descriptionText:(NSString*)descriptionText
                                layoutType:(IconDetailViewLayoutType)layoutType
                                badgeShape:(IconDetailViewBadgeShape)badgeShape;

// Returns the configuration for an `IconView` associated with this
// `IconDetailView`.
- (IconViewConfiguration*)iconViewConfiguration:(BOOL)inSquare;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_DETAIL_VIEW_CONFIGURATION_H_
