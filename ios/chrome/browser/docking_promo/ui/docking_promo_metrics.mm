// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/ui/docking_promo_metrics.h"

#import "base/metrics/histogram_functions.h"

const char kIOSDockingPromoActionHistogram[] = "IOS.DockingPromo.Action";

void RecordDockingPromoAction(IOSDockingPromoAction action) {
  base::UmaHistogramEnumeration(kIOSDockingPromoActionHistogram, action);
}
