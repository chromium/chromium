// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/model/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kBringYourOwnTabsIOS,
             "BringYourOwnTabsIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kBringYourOwnTabsIOSParam[] = "bottom-message";

BringYourOwnTabsPromptType GetBringYourOwnTabsPromptType() {
  if (base::FeatureList::IsEnabled(kBringYourOwnTabsIOS)) {
    bool showBottomMessagePrompt = base::GetFieldTrialParamByFeatureAsBool(
        kBringYourOwnTabsIOS, kBringYourOwnTabsIOSParam, false);
    return showBottomMessagePrompt ? BringYourOwnTabsPromptType::kBottomMessage
                                   : BringYourOwnTabsPromptType::kHalfSheet;
  }
  return BringYourOwnTabsPromptType::kDisabled;
}
