// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/feature_flags.h"
#import "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kWhatsNewIOS, "WhatsNewIOS", base::FEATURE_ENABLED_BY_DEFAULT);

const char kWhatsNewModuleBasedLayoutParam[] = "whats_new_module_based_layout";

bool IsWhatsNewModuleBasedLayout() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kWhatsNewIOS, kWhatsNewModuleBasedLayoutParam, true);
}
