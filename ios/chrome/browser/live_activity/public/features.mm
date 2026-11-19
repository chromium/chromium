// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/live_activity/public/features.h"

BASE_FEATURE(kIOSLiveActivityManager, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSLiveActivityManagerEnabled() {
  return base::FeatureList::IsEnabled(kIOSLiveActivityManager);
}
