// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation ChromeTableViewStyler

- (instancetype)init {
  if ((self = [super init])) {
    _tableViewBackgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    _cellBackgroundColor =
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  }
  return self;
}

@end
