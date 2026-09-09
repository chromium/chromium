// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/features.h"

#import "ios/chrome/app/background_mode_buildflags.h"

BASE_FEATURE(kEnableBackgroundContinuedProcessing,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsBackgroundContinuedProcessingEnabled() {
#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  if (@available(iOS 26.0, *)) {
    return base::FeatureList::IsEnabled(kEnableBackgroundContinuedProcessing);
  }
#endif
  return false;
}
