// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Constants related to icon sizing.
constexpr CGFloat kIconSize = 22;

}  // namespace

@implementation IconViewConfiguration

+ (instancetype)configurationWithSymbol:(Symbol)symbol {
  return [[IconViewConfiguration alloc]
             initWithSymbol:symbol
                       icon:nil
                 iconSource:IconViewSourceType::kSymbol
         symbolColorPalette:@[ [UIColor whiteColor] ]
      symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
                  iconWidth:kIconSize
              compactLayout:NO
                   inSquare:YES];
}

+ (instancetype)configurationWithImageNamed:(NSString*)imageName {
  return [[IconViewConfiguration alloc]
             initWithSymbol:SymbolNone
                       icon:imageName
                 iconSource:IconViewSourceType::kImage
         symbolColorPalette:@[ [UIColor whiteColor] ]
      symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
                  iconWidth:kIconSize
              compactLayout:NO
                   inSquare:YES];
}

#pragma mark - Private Initializers

- (instancetype)initWithSymbol:(Symbol)symbol
                          icon:(NSString*)iconName
                    iconSource:(IconViewSourceType)iconSource
            symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
         symbolBackgroundColor:(UIColor*)symbolBackgroundColor
                     iconWidth:(CGFloat)iconWidth
                 compactLayout:(BOOL)compactLayout
                      inSquare:(BOOL)inSquare {
  if ((self = [super init])) {
    _symbol = symbol;
    _iconName = [iconName copy];
    _iconSource = iconSource;
    _symbolColorPalette = [symbolColorPalette copy];
    _symbolBackgroundColor = symbolBackgroundColor;
    _iconWidth = iconWidth;
    _compactLayout = compactLayout;
    _inSquare = inSquare;
  }
  return self;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  return [[IconViewConfiguration allocWithZone:zone]
             initWithSymbol:self.symbol
                       icon:self.iconName
                 iconSource:self.iconSource
         symbolColorPalette:self.symbolColorPalette
      symbolBackgroundColor:self.symbolBackgroundColor
                  iconWidth:self.iconWidth
              compactLayout:self.compactLayout
                   inSquare:self.inSquare];
}

@end
