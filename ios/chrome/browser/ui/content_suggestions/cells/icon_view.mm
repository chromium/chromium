// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/icon_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Constants related to icon container sizing.
constexpr CGFloat kIconContainerSize = 30;
constexpr CGFloat kIconSquareContainerRadius = 7;

// Returns a UIImageView for the given SF Symbol with color(s) `color_palette`,
// using `default_symbol`.
UIImageView* IconForSymbol(NSString* symbol,
                           CGFloat symbol_width,
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

  [NSLayoutConstraint activateConstraints:@[
    [icon.widthAnchor constraintEqualToConstant:symbol_width],
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
  // The symbol name for the icon.
  NSString* _symbol;
  // The color palette of the icon.
  NSArray<UIColor*>* _symbolColorPalette;
  // The background color of the icon.
  UIColor* _symbolBackgroundColor;
  // The width of the symbol.
  CGFloat _symbolWidth;
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
                   symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
                symbolBackgroundColor:(UIColor*)symbolBackgroundColor
                          symbolWidth:(CGFloat)symbolWidth
                        compactLayout:(BOOL)compactLayout
                             inSquare:(BOOL)inSquare {
  if ((self = [super init])) {
    _symbol = defaultSymbolName;
    _symbolColorPalette = symbolColorPalette;
    _symbolBackgroundColor = symbolBackgroundColor;
    _symbolWidth = symbolWidth;
    _defaultSymbol = YES;
    _compactLayout = compactLayout;
    _inSquare = inSquare;
  }

  return self;
}

- (instancetype)initWithDefaultSymbol:(NSString*)defaultSymbolName
                          symbolWidth:(CGFloat)symbolWidth
                        compactLayout:(BOOL)compactLayout
                             inSquare:(BOOL)inSquare {
  return [self initWithDefaultSymbol:defaultSymbolName
                  symbolColorPalette:@[ [UIColor whiteColor] ]
               symbolBackgroundColor:[UIColor colorNamed:kBlue500Color]
                         symbolWidth:symbolWidth
                       compactLayout:compactLayout
                            inSquare:inSquare];
}

- (instancetype)initWithCustomSymbol:(NSString*)customSymbolName
                  symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
               symbolBackgroundColor:(UIColor*)symbolBackgroundColor
                         symbolWidth:(CGFloat)symbolWidth
                       compactLayout:(BOOL)compactLayout
                            inSquare:(BOOL)inSquare {
  if ((self = [super init])) {
    _symbol = customSymbolName;
    _symbolColorPalette = symbolColorPalette;
    _symbolBackgroundColor = symbolBackgroundColor;
    _symbolWidth = symbolWidth;
    _defaultSymbol = NO;
    _compactLayout = compactLayout;
    _inSquare = inSquare;
  }
  return self;
}

- (instancetype)initWithCustomSymbol:(NSString*)customSymbolName
                         symbolWidth:(CGFloat)symbolWidth
                       compactLayout:(BOOL)compactLayout
                            inSquare:(BOOL)inSquare {
  return [self initWithCustomSymbol:customSymbolName
                 symbolColorPalette:@[ [UIColor whiteColor] ]
              symbolBackgroundColor:[UIColor colorNamed:kBlue500Color]
                        symbolWidth:symbolWidth
                      compactLayout:compactLayout
                           inSquare:inSquare];
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
  UIImageView* icon =
      IconForSymbol(_symbol, _symbolWidth, _defaultSymbol, _symbolColorPalette);

  return IconInSquareContainer(icon, _symbolBackgroundColor);
}

@end
