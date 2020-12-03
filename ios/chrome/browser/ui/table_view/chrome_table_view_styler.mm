// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeTableViewStyler

- (instancetype)init {
  if ((self = [super init])) {
    if (base::FeatureList::IsEnabled(kSettingsRefresh)) {
      _tableViewBackgroundColor =
          [UIColor colorNamed:kSecondaryBackgroundColor];
      _cellBackgroundColor = UIColor.cr_secondarySystemGroupedBackgroundColor;
    } else {
      _tableViewBackgroundColor = UIColor.cr_systemGroupedBackgroundColor;
    }
  }
  return self;
}

@end
