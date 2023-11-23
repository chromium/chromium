// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"

#import "ios/chrome/browser/shared/public/features/features.h"

ToolbarType DefaultSelectedOmniboxPosition() {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAny));
  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxPromoDefaultPosition,
      kBottomOmniboxPromoDefaultPositionParam);
  if (featureParam == kBottomOmniboxPromoDefaultPositionParamTop) {
    return ToolbarType::kPrimary;
  } else if (featureParam == kBottomOmniboxPromoDefaultPositionParamBottom) {
    return ToolbarType::kSecondary;
  }
  return ToolbarType::kPrimary;
}
