// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void RecordHomeAction(IOSHomeActionType type, bool isStartSurface) {
  if (isStartSurface) {
    UMA_HISTOGRAM_ENUMERATION(kActionOnStartHistogram, type);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kActionOnNTPHistogram, type);
  }
}
