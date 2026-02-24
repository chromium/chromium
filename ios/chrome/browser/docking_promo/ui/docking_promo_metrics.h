// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_METRICS_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_METRICS_H_

enum class IOSDockingPromoEligibility;

// Enum for metrics-releated histogram: IOS.DockingPromo.Action.
//
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSDockingPromoAction {
  kToggleAppearance_OBSOLETE = 0,  // Feature removed.
  kGotIt = 1,
  kRemindMeLater_OBSOLETE = 2,  // Feature removed.
  kDismissViaSwipe = 3,
  kDismissViaNoThanks_OBSOLETE = 4,  // Feature removed.
  kMaxValue = kDismissViaNoThanks_OBSOLETE,
};

// Records a user action on the promo, segmented by their eligibility type.
void RecordDockingPromoAction(IOSDockingPromoAction action,
                              IOSDockingPromoEligibility eligibility);

// Records the eligibility status of the user at the moment the Docking Promo
// impression occurs.
void RecordDockingPromoImpression(IOSDockingPromoEligibility eligibility);

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_METRICS_H_
