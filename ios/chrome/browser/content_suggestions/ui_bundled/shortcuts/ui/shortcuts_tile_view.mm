// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_tile_view.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_action_item.h"

@implementation ContentSuggestionsShortcutTileView

- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedActionItem*)config {
  CHECK([config isKindOfClass:ShortcutsActionItem.class]);
  return [super initWithConfiguration:config];
}

- (void)shortcutsItemConfigDidChange:(ShortcutsActionItem*)config {
  ShortcutsActionItem* currentConfig =
      base::apple::ObjCCastStrict<ShortcutsActionItem>(self.config);
  if (config.collectionShortcutType == currentConfig.collectionShortcutType) {
    [self updateConfiguration:config];
  }
}

@end
