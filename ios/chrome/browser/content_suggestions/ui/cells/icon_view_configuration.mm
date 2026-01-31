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

+ (instancetype)configurationWithSymbolNamed:(NSString*)symbolName {
  return [[IconViewConfiguration alloc]
               initWithIcon:symbolName
                 iconSource:IconViewSourceType::kSymbol
         symbolColorPalette:@[ [UIColor whiteColor] ]
      symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
                  iconWidth:kIconSize
              defaultSymbol:NO
              compactLayout:NO
                   inSquare:YES];
}

+ (instancetype)configurationWithImageNamed:(NSString*)imageName {
  return [[IconViewConfiguration alloc]
               initWithIcon:imageName
                 iconSource:IconViewSourceType::kImage
         symbolColorPalette:@[ [UIColor whiteColor] ]
      symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
                  iconWidth:kIconSize
              defaultSymbol:NO
              compactLayout:NO
                   inSquare:YES];
}

#pragma mark - Private Initializers

- (instancetype)initWithIcon:(NSString*)iconName
                  iconSource:(IconViewSourceType)iconSource
          symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
       symbolBackgroundColor:(UIColor*)symbolBackgroundColor
                   iconWidth:(CGFloat)iconWidth
               defaultSymbol:(BOOL)defaultSymbol
               compactLayout:(BOOL)compactLayout
                    inSquare:(BOOL)inSquare {
  if ((self = [super init])) {
    _iconName = [iconName copy];
    _iconSource = iconSource;
    _symbolColorPalette = [symbolColorPalette copy];
    _symbolBackgroundColor = symbolBackgroundColor;
    _iconWidth = iconWidth;
    _defaultSymbol = defaultSymbol;
    _compactLayout = compactLayout;
    _inSquare = inSquare;
  }
  return self;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  return [[IconViewConfiguration allocWithZone:zone]
               initWithIcon:self.iconName
                 iconSource:self.iconSource
         symbolColorPalette:self.symbolColorPalette
      symbolBackgroundColor:self.symbolBackgroundColor
                  iconWidth:self.iconWidth
              defaultSymbol:self.defaultSymbol
              compactLayout:self.compactLayout
                   inSquare:self.inSquare];
}

@end
