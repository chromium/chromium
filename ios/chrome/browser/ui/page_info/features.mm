// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/features.h"

BASE_FEATURE(kRevampPageInfoIos,
             "RevampPageInfoIos",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsRevampPageInfoIosEnabled() {
  return base::FeatureList::IsEnabled(kRevampPageInfoIos);
}
