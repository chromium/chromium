// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kEnableCloseAllTabsConfirmation{
    "EnableCloseAllTabsConfirmation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabGridContextMenu{"TabGridContextMenu",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabsBulkActions{"TabsBulkActions",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

bool IsCloseAllTabsConfirmationEnabled() {
  return base::FeatureList::IsEnabled(kEnableCloseAllTabsConfirmation);
}

bool IsTabGridContextMenuEnabled() {
  if (@available(iOS 13, *)) {
    return base::FeatureList::IsEnabled(kTabGridContextMenu);
  }
  return false;
}

bool IsTabsBulkActionsEnabled() {
  if (@available(iOS 14, *)) {
    return base::FeatureList::IsEnabled(kTabsBulkActions);
  }
  return false;
}
