// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file only contains the definition of
// IOSChromeFieldTrials::EnableRuntimeMutableFeatures(). It lives in its own
// file (rather than in ios_chrome_field_trials.mm) so that it can have
// dedicated OWNERS. See
// //components/variations/RUNTIME_MUTABLE_FEATURES_OWNERS.

#import "base/feature_list.h"
#import "components/variations/platform_field_trials.h"
#import "ios/chrome/browser/flags/ios_chrome_field_trials.h"

void IOSChromeFieldTrials::EnableRuntimeMutableFeatures(
    base::FeatureList* feature_list) {
  variations::PlatformFieldTrials::EnableRuntimeMutableFeatures(feature_list);

  // Add calls to enable runtime-mutable features here.
}
