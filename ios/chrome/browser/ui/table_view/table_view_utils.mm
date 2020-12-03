// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UITableViewStyle ChromeTableViewStyle() {
  if (@available(iOS 13, *)) {
    if (base::FeatureList::IsEnabled(kSettingsRefresh) && !IsSmallDevice())
      return UITableViewStyleInsetGrouped;
  }
  return UITableViewStyleGrouped;
}
