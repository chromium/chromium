// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kNewOverflowMenu{"NewOverflowMenu",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNewOverflowMenuEnabled() {
  if (@available(iOS 15, *)) {
    return base::FeatureList::IsEnabled(kNewOverflowMenu);
  }
  // The new overflow menu isn't available on iOS <= 14 because it relies on
  // |UISheetPresentationController|, which was introduced in iOS 15.
  return false;
}
