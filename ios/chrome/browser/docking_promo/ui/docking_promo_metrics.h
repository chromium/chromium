// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_METRICS_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_METRICS_H_

// Name of the histogram that logs actions taken on the Docking Promo after it's
// displayed to the user.
extern const char kIOSDockingPromoActionHistogram[];

// Enum for metrics-releated histogram: IOS.DockingPromo.Action.
//
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSDockingPromoAction : int {
  kToggleAppearance = 0,
  kGotIt = 1,
  kRemindMeLater = 2,
  kDismissViaSwipe = 3,
  kMaxValue = kDismissViaSwipe,
};

// Record Docking Promo `action` metric in the histogram
// `kIOSDockingPromoActionHistogram`.
void RecordDockingPromoAction(IOSDockingPromoAction action);

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_METRICS_H_
