// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Returns true if the Thumb Strip feature is enabled and the device is an iPad,
// and Voice Over isn't active. There's no clean way to open and navigate
// thumbstrip with VO.
bool IsThumbStripEnabled() {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) &&
         base::FeatureList::IsEnabled(kExpandedTabStrip) &&
         !UIAccessibilityIsVoiceOverRunning();
}

bool ShowThumbStripInTraitCollection(UITraitCollection* trait_collection) {
  return IsThumbStripEnabled() && IsRegularXRegularSizeClass(trait_collection);
}
