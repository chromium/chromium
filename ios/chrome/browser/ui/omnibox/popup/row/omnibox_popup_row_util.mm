// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_util.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ui/base/device_form_factor.h"

BOOL CanUseOmniboxLayoutGuide() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return YES;
  } else {
    return !base::FeatureList::IsEnabled(kOmniboxSuggestionsRTLImprovements);
  }
}

BOOL ShouldApplyOmniboxLayoutGuide(UITraitCollection* traitCollection) {
  CHECK(CanUseOmniboxLayoutGuide());

  if (IsRegularXRegularSizeClass(traitCollection)) {
    return !IsIpadPopoutOmniboxEnabled();
  } else {
    return !base::FeatureList::IsEnabled(kOmniboxSuggestionsRTLImprovements);
  }
}
