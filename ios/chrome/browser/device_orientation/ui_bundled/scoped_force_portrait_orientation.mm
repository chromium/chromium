// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"

#import "ios/chrome/browser/device_orientation/ui_bundled/portait_orientation_manager.h"

ScopedForcePortraitOrientation::ScopedForcePortraitOrientation(
    id<PortraitOrientationManager> manager)
    : manager_(manager) {
  [manager_ incrementForcePortraitOrientationCounter];
}

ScopedForcePortraitOrientation::~ScopedForcePortraitOrientation() {
  [manager_ decrementForcePortraitOrientationCounter];
}
