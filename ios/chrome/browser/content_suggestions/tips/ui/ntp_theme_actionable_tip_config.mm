// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_theme_actionable_tip_config.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation NTPThemeActionableTipConfig

- (instancetype)init {
  if ((self = [super init])) {
    self.shouldShowSeeMore = YES;
  }
  return self;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  NTPThemeActionableTipConfig* config = [super copyWithZone:zone];
  config.colorPalettes = [self.colorPalettes copyWithZone:zone];
  config.backgroundImages = [self.backgroundImages copyWithZone:zone];
  config.selectedIndex = self.selectedIndex;
  config.audience = self.audience;
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kTipsNTPThemeActionable;
}

- (BOOL)hasDifferentContentsFromConfig:(MagicStackModule*)config {
  if ([super hasDifferentContentsFromConfig:config]) {
    return YES;
  }
  NTPThemeActionableTipConfig* otherConfig =
      static_cast<NTPThemeActionableTipConfig*>(config);
  return ![self.backgroundImages isEqualToArray:otherConfig.backgroundImages] ||
         self.selectedIndex != otherConfig.selectedIndex;
}

@end
