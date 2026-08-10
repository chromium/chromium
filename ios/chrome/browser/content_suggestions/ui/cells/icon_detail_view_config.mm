// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view_config.h"

#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"

@implementation IconDetailViewConfig {
  IconViewConfiguration* _iconViewConfiguration;
}

#pragma mark - Public

- (IconViewConfiguration*)iconViewConfiguration:(BOOL)inSquare {
  _iconViewConfiguration =
      [IconViewConfiguration configurationWithSymbol:self.symbol];
  _iconViewConfiguration.iconName = self.iconName;
  _iconViewConfiguration.iconSource = self.iconSource;
  _iconViewConfiguration.symbolColorPalette = self.symbolColorPalette;
  _iconViewConfiguration.symbolBackgroundColor = self.symbolBackgroundColor;
  _iconViewConfiguration.iconWidth = self.iconWidth;
  _iconViewConfiguration.compactLayout =
      (self.layoutType != IconDetailViewLayoutType::kHero);
  _iconViewConfiguration.inSquare = inSquare;
  return _iconViewConfiguration;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  IconDetailViewConfig* viewConfig = [[super copyWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  viewConfig.titleText = [self.titleText copy];
  viewConfig.descriptionText = [self.descriptionText copy];
  viewConfig.layoutType = self.layoutType;
  viewConfig.backgroundImage = self.backgroundImage;
  viewConfig.iconSource = self.iconSource;
  viewConfig.iconName = [self.iconName copy];
  viewConfig.symbol = self.symbol;
  viewConfig.symbolColorPalette = [self.symbolColorPalette copy];
  viewConfig.symbolBackgroundColor = self.symbolBackgroundColor;
  viewConfig.iconWidth = self.iconWidth;
  viewConfig.showCheckmark = self.showCheckmark;
  viewConfig.ntpBackgroundColorPalette = self.ntpBackgroundColorPalette;
  viewConfig.badgeSymbol = self.badgeSymbol;
  viewConfig.badgeColorPalette = [self.badgeColorPalette copy];
  viewConfig.badgeShapeConfig = self.badgeShapeConfig;
  viewConfig.badgeBackgroundColor = self.badgeBackgroundColor;
  viewConfig.accessibilityIdentifier = [self.accessibilityIdentifier copy];
  // LINT.ThenChange(icon_detail_view_config.h:Copy)
  return viewConfig;
}

@end
