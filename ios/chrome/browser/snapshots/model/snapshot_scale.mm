// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_scale.h"

#import "ui/base/device_form_factor.h"

@implementation SnapshotImageScale

+ (ImageScale)imageScaleForDevice {
  // On handset, the color snapshot is used for the stack view, so the scale of
  // the snapshot images should match the scale of the device.
  // On tablet, the color snapshot is only used to generate the grey snapshot,
  // which does not have to be high quality, so use scale of 1.0 on all tablets.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return kImageScale1X;
  }

  // Cap snapshot resolution to 2x to reduce the amount of memory used.
  return [UIScreen mainScreen].scale == 1.0 ? kImageScale1X : kImageScale2X;
}

+ (CGFloat)floatImageScaleForDevice {
  switch ([self imageScaleForDevice]) {
    case kImageScale1X:
      return 1.0;
    case kImageScale2X:
      return 2.0;
  }
}

@end
