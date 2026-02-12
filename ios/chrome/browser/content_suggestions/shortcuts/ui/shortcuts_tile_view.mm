// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_tile_view.h"

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_action_item.h"

@implementation ContentSuggestionsShortcutTileView

- (instancetype)initWithConfiguration:(ContentSuggestionsActionItem*)config {
  CHECK([config isKindOfClass:ShortcutsActionItem.class]);
  return [super initWithConfiguration:config];
}

@end
