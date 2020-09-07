// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace fullscreen {
namespace features {

const base::Feature kSmoothScrollingDefault{"FullscreenSmoothScrollingDefault",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFullscreenControllerBrowserScoped{
    "FullscreenControllerBrowserScoped", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldUseSmoothScrolling() {
  return base::FeatureList::IsEnabled(kSmoothScrollingDefault);
}

bool ShouldScopeFullscreenControllerToBrowser() {
  if (IsMultiwindowSupported()) {
    return true;
  }

  return base::FeatureList::IsEnabled(kFullscreenControllerBrowserScoped);
}

}  // namespace features
}  // namespace fullscreen
