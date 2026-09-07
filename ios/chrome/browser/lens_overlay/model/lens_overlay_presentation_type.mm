// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_presentation_type.h"

#import <string>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

namespace lens {

ContainerPresentationType ContainerPresentationFor(
    id<UITraitEnvironment> environment) {
  return IsRegularXRegularSizeClass(environment)
             ? ContainerPresentationType::kContentAreaCover
             : ContainerPresentationType::kFullscreenCover;
}

ResultPagePresentationType ResultPagePresentationFor(
    id<UITraitEnvironment> environment,
    bool is_lvf) {
  if (is_lvf && ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
      base::FeatureList::IsEnabled(kEnableLensOnIPad)) {
    std::string style = base::GetFieldTrialParamValueByFeature(
        kEnableLensOnIPad, kEnableLensOnIPadPresentationStyleParam);
    if (style == kEnableLensOnIPadPresentationStyleSidePanel) {
      return ResultPagePresentationType::kSidePanel;
    }
    return ResultPagePresentationType::kEdgeAttachedBottomSheet;
  }
  return IsRegularXRegularSizeClass(environment)
             ? ResultPagePresentationType::kSidePanel
             : ResultPagePresentationType::kEdgeAttachedBottomSheet;
}

}  // namespace lens
