// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"

#import "components/password_manager/core/common/password_manager_features.h"

BASE_FEATURE(kNewOverflowMenu,
             "NewOverflowMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNewOverflowMenuEnabled() {
  return base::FeatureList::IsEnabled(kNewOverflowMenu);
}
