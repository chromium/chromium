// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view_configuration.h"

#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"

@implementation IconDetailViewConfiguration {
  IconViewConfiguration* _iconViewConfiguration;
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
  IconDetailViewConfiguration* viewConfig = [[super copyWithZone:zone] init];
  viewConfig.titleText = [self.titleText copy];
  viewConfig.descriptionText = [self.descriptionText copy];
  viewConfig.layoutType = self.layoutType;
  viewConfig.backgroundImage = self.backgroundImage;
  viewConfig.iconName = [self.iconName copy];
  viewConfig.iconSource = self.iconSource;
  viewConfig.symbolColorPalette = [self.symbolColorPalette copy];
  viewConfig.symbolBackgroundColor = self.symbolBackgroundColor;
  viewConfig.usesDefaultSymbol = self.usesDefaultSymbol;
  viewConfig.iconWidth = self.iconWidth;
  viewConfig.showCheckmark = self.showCheckmark;
  viewConfig.ntpBackgroundColorPalette = self.ntpBackgroundColorPalette;
  viewConfig.badgeSymbolName = [self.badgeSymbolName copy];
  viewConfig.badgeColorPalette = [self.badgeColorPalette copy];
  viewConfig.badgeShapeConfig = self.badgeShapeConfig;
  viewConfig.badgeBackgroundColor = self.badgeBackgroundColor;
  viewConfig.badgeUsesDefaultSymbol = self.badgeUsesDefaultSymbol;
  viewConfig.accessibilityIdentifier = [self.accessibilityIdentifier copy];
  return viewConfig;
}

@end
