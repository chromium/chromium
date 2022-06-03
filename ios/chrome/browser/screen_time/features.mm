// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screen_time/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kScreenTimeIntegration{"ScreenTimeIntegration",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool IsScreenTimeIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kScreenTimeIntegration);
}
