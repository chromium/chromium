// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_item_icon.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Constants related to icon sizing.
constexpr CGFloat kIconSize = 22;
constexpr CGFloat kIconContainerSize = 30;
constexpr CGFloat kIconSquareContainerRadius = 7;
// NOTE: The Safety Check (Magic Stack) module uses a slightly larger version of
// the SF Symbols Password icon in its design.
constexpr CGFloat kPasswordIconSize = 26;

// Returns an icon-specific width given `symbol`.
CGFloat IconWidthForSymbolName(NSString* symbol) {
  if (symbol == kPasswordSymbol) {
    return kPasswordIconSize;
  }

  return kIconSize;
}

// Returns a UIImageView for the given SF Symbol with color(s) `color_palette`,
// using `default_symbol`.
UIImageView* IconForSymbol(NSString* symbol,
                           BOOL default_symbol,
                           NSArray<UIColor*>* color_palette = nil) {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];

  if (color_palette) {
    UIImageSymbolConfiguration* colorConfig = [UIImageSymbolConfiguration
        configurationWithPaletteColors:color_palette];

    config = [config configurationByApplyingConfiguration:colorConfig];
  }

  UIImage* image = default_symbol
                       ? DefaultSymbolWithConfiguration(symbol, config)
                       : CustomSymbolWithConfiguration(symbol, config);

  UIImageView* icon = [[UIImageView alloc] initWithImage:image];

  icon.translatesAutoresizingMaskIntoConstraints = NO;

  CGFloat icon_width = IconWidthForSymbolName(symbol);

  [NSLayoutConstraint activateConstraints:@[
    [icon.widthAnchor constraintEqualToConstant:icon_width],
    [icon.heightAnchor constraintEqualToAnchor:icon.widthAnchor],
  ]];

  return icon;
}

// Returns a UIView for the given `icon` wrapped in a container with
// `containerColor`.
UIView* IconInSquareContainer(UIImageView* icon, NSString* containerColor) {
  UIView* square_view = [[UIView alloc] init];

  square_view.translatesAutoresizingMaskIntoConstraints = NO;
  square_view.layer.cornerRadius = kIconSquareContainerRadius;
  square_view.backgroundColor = [UIColor colorNamed:containerColor];

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

@implementation SafetyCheckItemIcon {
  // The symbol name for the icon.
  NSString* _symbol;
  // YES if `_symbol` is a default symbol name. (NO if `_symbol` is a custom
  // symbol name.)
  BOOL _defaultSymbol;
  // YES if this icon should configure itself in a smaller, compact
  // size.
  BOOL _compactLayout;
  // YES if this icon should place itself within a square enclosure.
  BOOL _inSquare;
  // The view containing the icon.
  UIView* _icon;
}

- (instancetype)initWithDefaultSymbol:(NSString*)defaultSymbolName
                        compactLayout:(BOOL)compactLayout
                             inSquare:(BOOL)inSquare {
  if ((self = [super init])) {
    _symbol = defaultSymbolName;
    _defaultSymbol = YES;
    _compactLayout = compactLayout;
    _inSquare = inSquare;
  }

  return self;
}

- (instancetype)initWithCustomSymbol:(NSString*)customSymbolName
                       compactLayout:(BOOL)compactLayout
                            inSquare:(BOOL)inSquare {
  if ((self = [super init])) {
    _symbol = customSymbolName;
    _defaultSymbol = NO;
    _compactLayout = compactLayout;
    _inSquare = inSquare;
  }

  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Private

// Creates all views for the icon of a particular check row in the Safety Check
// (Magic Stack) module.
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

// Creates the type-specific icon.
- (UIView*)createIcon {
  // Compact, in-square icons are displayed in light blue.
  if (_inSquare && _compactLayout) {
    UIImageView* icon = IconForSymbol(_symbol, _defaultSymbol,
                                      @[ [UIColor colorNamed:kBlue500Color] ]);

    return IconInSquareContainer(icon, kBlueHaloColor);
  }

  // Non-compact, in-square icons are displayed in white.
  if (_inSquare) {
    UIImageView* icon =
        IconForSymbol(_symbol, _defaultSymbol, @[ [UIColor whiteColor] ]);

    return IconInSquareContainer(icon, kBlue500Color);
  }

  // By default, display icons in gray, with a square container.
  return IconForSymbol(_symbol, _defaultSymbol,
                       @[ [UIColor colorNamed:kGrey500Color] ]);
}

@end
