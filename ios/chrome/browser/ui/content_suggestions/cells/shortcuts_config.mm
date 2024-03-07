// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_config.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

@implementation ShortcutsConfig

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kShortcuts;
}

@end
