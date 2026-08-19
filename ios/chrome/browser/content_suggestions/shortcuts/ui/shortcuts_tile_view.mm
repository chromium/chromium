// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_tile_view.h"

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_action_item.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation ContentSuggestionsShortcutTileView

- (instancetype)initWithConfiguration:(ContentSuggestionsActionItem*)config {
  CHECK([config isKindOfClass:ShortcutsActionItem.class]);
  return [super initWithConfiguration:config];
}

#pragma mark - ContentSuggestionsActionTileView

- (void)applyBackgroundTheme {
  BOOL hasImageBackground =
      [self.traitCollection boolForNewTabPageImageBackgroundTrait];
  if (!hasImageBackground || !IsNewTabPageUICleanupEnabled()) {
    [super applyBackgroundTheme];
    return;
  }

  self.imageBackgroundView.tintColor =
      [UIColor colorNamed:kNewTabPageBackgroundColor];
  self.iconView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
}

@end
