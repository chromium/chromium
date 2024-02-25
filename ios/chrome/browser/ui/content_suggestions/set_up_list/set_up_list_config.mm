// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_config.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"

@implementation SetUpListConfig

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  if (_shouldShowCompactModule) {
    return ContentSuggestionsModuleType::kCompactedSetUpList;
  }
  SetUpListItemViewData* setUpListItem = _setUpListItems[0];
  return SetUpListModuleTypeForSetUpListType(setUpListItem.type);
}

@end
