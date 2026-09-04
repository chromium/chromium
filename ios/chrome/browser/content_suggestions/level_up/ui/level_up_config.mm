// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/level_up/ui/level_up_config.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation LevelUpConfig

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kLevelUp;
}

- (BOOL)hasDifferentContentsFromConfig:(MagicStackModule*)config {
  if ([super hasDifferentContentsFromConfig:config]) {
    return YES;
  }
  LevelUpConfig* levelUpConfig = static_cast<LevelUpConfig*>(config);
  return self.progressCompleted != levelUpConfig.progressCompleted ||
         self.progressTotal != levelUpConfig.progressTotal ||
         (self.titleText != levelUpConfig.titleText &&
          ![self.titleText isEqualToString:levelUpConfig.titleText]) ||
         (self.descriptionText != levelUpConfig.descriptionText &&
          ![self.descriptionText
              isEqualToString:levelUpConfig.descriptionText]);
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  LevelUpConfig* config = [super copyWithZone:zone];
  config.titleText = self.titleText;
  config.descriptionText = self.descriptionText;
  config.progressTotal = self.progressTotal;
  config.progressCompleted = self.progressCompleted;
  return config;
}

@end
