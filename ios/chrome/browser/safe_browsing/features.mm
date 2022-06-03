// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/features.h"

#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "components/password_manager/core/common/password_manager_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsPasswordReuseDetectionEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordReuseDetectionEnabled);
}