// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_shortcut_tile_view.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_shortcut_item.h"

@implementation ContentSuggestionsShortcutTileView

- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedActionItem*)config {
  CHECK([config isKindOfClass:ContentSuggestionsShortcutItem.class]);
  return [super initWithConfiguration:config];
}

- (void)shortcutsItemConfigDidChange:(ContentSuggestionsShortcutItem*)config {
  ContentSuggestionsShortcutItem* currentConfig =
      base::apple::ObjCCastStrict<ContentSuggestionsShortcutItem>(self.config);
  if (config.collectionShortcutType == currentConfig.collectionShortcutType) {
    [self updateConfiguration:config];
  }
}

@end
