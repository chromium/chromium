// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_config.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation ShortcutsConfig

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kShortcuts;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  ShortcutsConfig* copy = [[super copyWithZone:zone] init];
  copy.shortcutItems = self.shortcutItems;
  copy.consumerSource = self.consumerSource;
  copy.commandHandler = self.commandHandler;
  return copy;
}

@end
