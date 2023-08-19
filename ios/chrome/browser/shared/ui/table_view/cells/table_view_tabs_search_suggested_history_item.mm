// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_tabs_search_suggested_history_item.h"

#import "base/format_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation TableViewTabsSearchSuggestedHistoryItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTabsSearchSuggestedHistoryCell class];
    self.image = [[UIImage imageNamed:@"suggested_action_history"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    self.title = l10n_util::GetNSString(
        IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_HISTORY_UNKNOWN_RESULT_COUNT);
    self.accessibilityIdentifier = kTableViewTabsSearchSuggestedHistoryItemId;
  }
  return self;
}

@end

@implementation TableViewTabsSearchSuggestedHistoryCell

- (void)updateHistoryResultsCount:(size_t)resultsCount {
  NSString* matchesStr = [NSString stringWithFormat:@"%" PRIuS, resultsCount];
  self.textLabel.text = l10n_util::GetNSStringF(
      IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_HISTORY,
      base::SysNSStringToUTF16(matchesStr));
}

@end
