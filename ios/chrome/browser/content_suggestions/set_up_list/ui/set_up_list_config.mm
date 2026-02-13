// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/set_up_list/ui/set_up_list_config.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/ui/set_up_list_item_view_data.h"

@implementation SetUpListConfig

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  if (_shouldShowCompactModule) {
    return ContentSuggestionsModuleType::kCompactedSetUpList;
  }
  SetUpListItemViewData* setUpListItem = _setUpListItems[0];
  return SetUpListModuleTypeForSetUpListType(setUpListItem.type);
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  SetUpListConfig* copy = [[super copyWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  copy.setUpListItems = self.setUpListItems;
  copy.shouldShowCompactModule = self.shouldShowCompactModule;
  copy.setUpListConsumerSource = self.setUpListConsumerSource;
  copy.commandHandler = self.commandHandler;
  // LINT.ThenChange(set_up_list_config.h:Copy)
  return copy;
}

@end
