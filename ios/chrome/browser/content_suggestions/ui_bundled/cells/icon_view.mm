// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Constants related to icon container sizing.
constexpr CGFloat kIconContainerSize = 30;
constexpr CGFloat kIconSquareContainerRadius = 7;

// Returns a UIImageView for the given SF Symbol using
// `icon_view_configuration`.
UIImageView* IconForSymbol(
    const IconViewConfiguration* icon_view_configuration) {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];

  if (icon_view_configuration.symbolColorPalette) {
    UIImageSymbolConfiguration* colorConfig = [UIImageSymbolConfiguration
        configurationWithPaletteColors:icon_view_configuration
                                           .symbolColorPalette];

    config = [config configurationByApplyingConfiguration:colorConfig];
  }

  UIImage* image = icon_view_configuration.defaultSymbol
                       ? DefaultSymbolWithConfiguration(
                             icon_view_configuration.symbol, config)
                       : CustomSymbolWithConfiguration(
                             icon_view_configuration.symbol, config);

  // If no color palette is provided, make the symbol multicolor.
  if (!icon_view_configuration.symbolColorPalette) {
    image = MakeSymbolMulticolor(image);
  }

  UIImageView* icon = [[UIImageView alloc] initWithImage:image];

  icon.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [icon.widthAnchor
        constraintEqualToConstant:icon_view_configuration.symbolWidth],
    [icon.heightAnchor constraintEqualToAnchor:icon.widthAnchor],
  ]];

  return icon;
}

// Returns a UIView for the given `icon` wrapped in a container with
// `containerColor`.
UIView* IconInSquareContainer(UIImageView* icon, UIColor* containerColor) {
  UIView* square_view = [[UIView alloc] init];

  square_view.translatesAutoresizingMaskIntoConstraints = NO;
  square_view.layer.cornerRadius = kIconSquareContainerRadius;
  square_view.backgroundColor = containerColor;

  icon.contentMode = UIViewContentModeScaleAspectFit;

  [square_view addSubview:icon];

  AddSameCenterConstraints(icon, square_view);

  [NSLayoutConstraint activateConstraints:@[
    [square_view.widthAnchor constraintEqualToConstant:kIconContainerSize],
    [square_view.heightAnchor constraintEqualToAnchor:square_view.widthAnchor],
  ]];

  return square_view;
}

}  // namespace

@implementation IconView {
  // Configuration for this view.
  IconViewConfiguration* _configuration;
  // The view containing the icon.
  UIView* _icon;
}

- (instancetype)initWithConfiguration:(IconViewConfiguration*)configuration {
  if ((self = [super initWithFrame:CGRectZero])) {
    _configuration = [configuration copy];
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Private

// Creates all views for the icon.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;

  _icon = [self createIcon];

  [self addSubview:_icon];

  AddSameConstraints(self, _icon);
}

// Creates the icon.
- (UIView*)createIcon {
  UIImageView* icon = IconForSymbol(_configuration);

  return IconInSquareContainer(icon, _configuration.symbolBackgroundColor);
}

@end
