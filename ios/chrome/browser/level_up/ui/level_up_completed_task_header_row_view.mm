// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_completed_task_header_row_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation LevelUpCompletedTaskHeaderRowView {
  // The header title displaying the number of completed tasks.
  NSString* _titleText;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  return _titleText;
}

#pragma mark - Public

- (void)configureWithCompletedTasksCount:(NSInteger)count
                                expanded:(BOOL)expanded {
  _titleText =
      l10n_util::GetPluralNSStringF(IDS_IOS_LEVEL_UP_COMPLETED_TASKS, count);

  [self configureWithTitle:_titleText
               description:nil
           backgroundColor:[UIColor colorNamed:kSecondaryBackgroundColor]
           chevronExpanded:expanded
           separatorHidden:!expanded];
  self.accessibilityExpandedStatus =
      expanded ? UIAccessibilityExpandedStatusExpanded
               : UIAccessibilityExpandedStatusCollapsed;
}

- (void)setExpanded:(BOOL)expanded {
  [self setSeparatorHidden:!expanded];
  [self setChevronExpanded:expanded animated:YES];
  self.accessibilityExpandedStatus =
      expanded ? UIAccessibilityExpandedStatusExpanded
               : UIAccessibilityExpandedStatusCollapsed;
}

@end
