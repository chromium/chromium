// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/omnibox_position_util.h"

#import "base/stl_util.h"
#import "base/time/time.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ui/base/device_form_factor.h"


namespace omnibox {

bool ShouldFocusedOmniboxFollowSteadyStatePosition() {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE ||
      IsComposeboxIOSEnabled()) {
    return NO;
  }

  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxEvolution, kBottomOmniboxEvolutionParam);
  return feature_param ==
         kBottomOmniboxEvolutionParamEditStateFollowSteadyState;
}

bool ForceBottomOmniboxInEditState() {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE ||
      IsComposeboxIOSEnabled()) {
    return NO;
  }

  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxEvolution, kBottomOmniboxEvolutionParam);
  return feature_param ==
         kBottomOmniboxEvolutionParamForceBottomOmniboxEditState;
}

}  // namespace omnibox
