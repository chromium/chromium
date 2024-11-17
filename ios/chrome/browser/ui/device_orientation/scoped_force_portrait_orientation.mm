// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"

#import "ios/chrome/browser/ui/device_orientation/portait_orientation_manager.h"
#import "ui/base/device_form_factor.h"

ScopedForcePortraitOrientation::ScopedForcePortraitOrientation(
    id<PortraitOrientationManager> manager)
    : manager_(manager) {
  [manager_ incrementForcePortraitOrientationCounter];
}

ScopedForcePortraitOrientation::~ScopedForcePortraitOrientation() {
  [manager_ decrementForcePortraitOrientationCounter];
}

std::unique_ptr<ScopedForcePortraitOrientation>
ForcePortraitOrientationOnIphone(id<PortraitOrientationManager> manager) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return nullptr;
  }

  return std::make_unique<ScopedForcePortraitOrientation>(manager);
}
