// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view_configuration.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Constants related to icon sizing.
constexpr CGFloat kIconSize = 22;

}  // namespace

@implementation IconViewConfiguration

+ (instancetype)configurationWithSymbolNamed:(NSString*)symbolName {
  return [[IconViewConfiguration alloc]
             initWithSymbol:symbolName
         symbolColorPalette:@[ [UIColor whiteColor] ]
      symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
                symbolWidth:kIconSize
              defaultSymbol:NO
              compactLayout:NO
                   inSquare:YES];
}

#pragma mark - Private Initializers

- (instancetype)initWithSymbol:(NSString*)symbolName
            symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
         symbolBackgroundColor:(UIColor*)symbolBackgroundColor
                   symbolWidth:(CGFloat)symbolWidth
                 defaultSymbol:(BOOL)defaultSymbol
                 compactLayout:(BOOL)compactLayout
                      inSquare:(BOOL)inSquare {
  if ((self = [super init])) {
    _symbol = [symbolName copy];
    _symbolColorPalette = [symbolColorPalette copy];
    _symbolBackgroundColor = symbolBackgroundColor;
    _symbolWidth = symbolWidth;
    _defaultSymbol = defaultSymbol;
    _compactLayout = compactLayout;
    _inSquare = inSquare;
  }
  return self;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  return [[IconViewConfiguration allocWithZone:zone]
             initWithSymbol:self.symbol
         symbolColorPalette:self.symbolColorPalette
      symbolBackgroundColor:self.symbolBackgroundColor
                symbolWidth:self.symbolWidth
              defaultSymbol:self.defaultSymbol
              compactLayout:self.compactLayout
                   inSquare:self.inSquare];
}

@end
