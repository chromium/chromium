// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_completed_task_header_row_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation LevelUpCompletedTaskHeaderRowView

- (void)configureWithCompletedTasksCount:(NSInteger)count
                                expanded:(BOOL)expanded {
  NSString* titleText =
      l10n_util::GetPluralNSStringF(IDS_IOS_LEVEL_UP_COMPLETED_TASKS, count);

  [self configureWithTitle:titleText
               description:nil
           backgroundColor:[UIColor colorNamed:kSecondaryBackgroundColor]
           chevronExpanded:expanded
           separatorHidden:!expanded];
}

- (void)setExpanded:(BOOL)expanded {
  [self setSeparatorHidden:!expanded];
  [self setChevronExpanded:expanded animated:YES];
}

@end
