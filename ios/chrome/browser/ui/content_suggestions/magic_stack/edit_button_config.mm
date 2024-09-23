// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/edit_button_config.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

@implementation EditButtonConfig

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  // This config should never be used as a placeholder. This type is returned
  // since every config put in the Magic Stack's DiffableDataSource needs to
  // conform to the same ItemType.
  return ContentSuggestionsModuleType::kPlaceholder;
}

@end
