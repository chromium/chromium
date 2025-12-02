// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_detail_view_configuration.h"

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Constants related to icon sizing.
constexpr CGFloat kIconSize = 22;

// Constants related to Badge Icon sizing.
constexpr CGFloat kBadgeIconSize = 14;

// Constants related to Badge Icon container sizing and positioning.
constexpr CGFloat kBadgeIconCircleContainerRadius = 10;
constexpr CGFloat kBadgeIconSquareContainerRadius = 4;

}  // namespace

@implementation IconDetailViewConfiguration {
  IconViewConfiguration* _iconViewConfiguration;
}

+ (instancetype)configurationWithTitleText:(NSString*)titleText
                           descriptionText:(NSString*)descriptionText {
  BadgeShapeConfig defaultBadgeShapeConfig = {
      IconDetailViewBadgeShape::kCircle, kBadgeIconSize,
      kBadgeIconCircleContainerRadius,   kBadgeIconCircleContainerRadius,
      kBadgeIconCircleContainerRadius,   kBadgeIconCircleContainerRadius};

  return [[IconDetailViewConfiguration alloc]
                 initWithTitleText:titleText
                   descriptionText:descriptionText
                        layoutType:IconDetailViewLayoutType::kHero
                   backgroundImage:nil
                          iconName:kChromeProductSymbol
                        iconSource:IconViewSourceType::kSymbol
                symbolColorPalette:@[ [UIColor whiteColor] ]
             symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
      iconContainerBackgroundColor:[UIColor colorNamed:kGrey100Color]
                 usesDefaultSymbol:NO
                         iconWidth:kIconSize
                     showCheckmark:NO
                   badgeSymbolName:nil
                 badgeColorPalette:nil
                  badgeShapeConfig:defaultBadgeShapeConfig
              badgeBackgroundColor:nil
            badgeUsesDefaultSymbol:NO
           accessibilityIdentifier:nil];
}

+ (instancetype)configurationWithTitleText:(NSString*)titleText
                           descriptionText:(NSString*)descriptionText
                                layoutType:(IconDetailViewLayoutType)layoutType
                                badgeShape:
                                    (IconDetailViewBadgeShape)badgeShape {
  CGFloat cornerRadius = (badgeShape == IconDetailViewBadgeShape::kSquare)
                             ? kBadgeIconSquareContainerRadius
                             : kBadgeIconCircleContainerRadius;
  BadgeShapeConfig badgeShapeConfig = {badgeShape,   kBadgeIconSize,
                                       cornerRadius, cornerRadius,
                                       cornerRadius, cornerRadius};
  return [[IconDetailViewConfiguration alloc]
                 initWithTitleText:titleText
                   descriptionText:descriptionText
                        layoutType:layoutType
                   backgroundImage:nil
                          iconName:kChromeProductSymbol
                        iconSource:IconViewSourceType::kSymbol
                symbolColorPalette:@[ [UIColor whiteColor] ]
             symbolBackgroundColor:[UIColor colorNamed:kBackgroundColor]
      iconContainerBackgroundColor:[UIColor colorNamed:kGrey100Color]
                 usesDefaultSymbol:NO
                         iconWidth:kIconSize
                     showCheckmark:NO
                   badgeSymbolName:nil
                 badgeColorPalette:nil
                  badgeShapeConfig:badgeShapeConfig
              badgeBackgroundColor:nil
            badgeUsesDefaultSymbol:NO
           accessibilityIdentifier:nil];
}

#pragma mark - Private Initializers

- (instancetype)initWithTitleText:(NSString*)titleText
                  descriptionText:(NSString*)descriptionText
                       layoutType:(IconDetailViewLayoutType)layoutType
                  backgroundImage:(UIImage*)backgroundImage
                         iconName:(NSString*)iconName
                       iconSource:(IconViewSourceType)iconSource
               symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
            symbolBackgroundColor:(UIColor*)symbolBackgroundColor
     iconContainerBackgroundColor:(UIColor*)iconContainerBackgroundColor
                usesDefaultSymbol:(BOOL)usesDefaultSymbol
                        iconWidth:(CGFloat)iconWidth
                    showCheckmark:(BOOL)showCheckmark
                  badgeSymbolName:(NSString*)badgeSymbolName
                badgeColorPalette:(NSArray<UIColor*>*)badgeColorPalette
                 badgeShapeConfig:(BadgeShapeConfig)badgeShapeConfig
             badgeBackgroundColor:(UIColor*)badgeBackgroundColor
           badgeUsesDefaultSymbol:(BOOL)badgeUsesDefaultSymbol
          accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  if ((self = [super init])) {
    _titleText = [titleText copy];
    _descriptionText = [descriptionText copy];
    _layoutType = layoutType;
    _backgroundImage = backgroundImage;
    _iconName = [iconName copy];
    _iconSource = iconSource;
    _symbolColorPalette = [symbolColorPalette copy];
    _symbolBackgroundColor = symbolBackgroundColor;
    _iconContainerBackgroundColor = iconContainerBackgroundColor;
    _usesDefaultSymbol = usesDefaultSymbol;
    _iconWidth = iconWidth;
    _showCheckmark = showCheckmark;
    _badgeSymbolName = [badgeSymbolName copy];
    _badgeColorPalette = [badgeColorPalette copy];
    _badgeShapeConfig = badgeShapeConfig;
    _badgeBackgroundColor = badgeBackgroundColor;
    _badgeUsesDefaultSymbol = badgeUsesDefaultSymbol;
    _accessibilityIdentifier = [accessibilityIdentifier copy];
  }
  return self;
}

#pragma mark - Public

- (IconViewConfiguration*)iconViewConfiguration:(BOOL)inSquare {
  _iconViewConfiguration =
      [IconViewConfiguration configurationWithSymbolNamed:self.iconName];
  _iconViewConfiguration.iconName = self.iconName;
  _iconViewConfiguration.iconSource = self.iconSource;
  _iconViewConfiguration.symbolColorPalette = self.symbolColorPalette;
  _iconViewConfiguration.symbolBackgroundColor = self.symbolBackgroundColor;
  _iconViewConfiguration.iconWidth = self.iconWidth;
  _iconViewConfiguration.defaultSymbol = self.usesDefaultSymbol;
  _iconViewConfiguration.compactLayout =
      (self.layoutType != IconDetailViewLayoutType::kHero);
  _iconViewConfiguration.inSquare = inSquare;
  return _iconViewConfiguration;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  return [[IconDetailViewConfiguration allocWithZone:zone]
                 initWithTitleText:self.titleText
                   descriptionText:self.descriptionText
                        layoutType:self.layoutType
                   backgroundImage:self.backgroundImage
                          iconName:self.iconName
                        iconSource:self.iconSource
                symbolColorPalette:self.symbolColorPalette
             symbolBackgroundColor:self.symbolBackgroundColor
      iconContainerBackgroundColor:self.iconContainerBackgroundColor
                 usesDefaultSymbol:self.usesDefaultSymbol
                         iconWidth:self.iconWidth
                     showCheckmark:self.showCheckmark
                   badgeSymbolName:self.badgeSymbolName
                 badgeColorPalette:self.badgeColorPalette
                  badgeShapeConfig:self.badgeShapeConfig
              badgeBackgroundColor:self.badgeBackgroundColor
            badgeUsesDefaultSymbol:self.badgeUsesDefaultSymbol
           accessibilityIdentifier:self.accessibilityIdentifier];
}

@end
