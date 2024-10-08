// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"

void RecordLensEntrypointAvailable() {
  base::UmaHistogramBoolean("IOS.LocationBar.LensOverlayEntrypointAvailable",
                            true);
}

void RecordLensEntrypointHidden(IOSLocationBarLeadingIconType visible_icon) {
  base::UmaHistogramEnumeration("IOS.LocationBar.LensOverlayEntrypointHidden",
                                visible_icon);
}
