// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_config.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation ShortcutsConfig

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  ShortcutsConfig* config = [[super copyWithZone:zone] init];
  config.shortcutItems = [self.shortcutItems copy];
  config.commandHandler = self.commandHandler;
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kShortcuts;
}

@end
