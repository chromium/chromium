// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_METRICS_UTIL_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_METRICS_UTIL_H_

#import "base/time/time.h"

// Possible action for a non modal promo.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NonModalPromoAction {
  kAccepted = 0,
  kAppear = 1,
  kDismiss = 2,
  kTimeout = 3,
  kBackgroundCancel = 4,
  kMaxValue = kBackgroundCancel
};

// The reason why a non modal promo was triggered.
enum class NonModalPromoTriggerType {
  kUnknown = 0,
  kPastedLink,
  kGrowthKitOpen,
  kShare,
};

// Logs the interesting metrics for an action happening in a non modal default
// promo.
void LogNonModalPromoAction(NonModalPromoAction action,
                            NonModalPromoTriggerType type,
                            NSInteger impression_number);

// Logs the time a non modal promo was on screen.
void LogNonModalTimeOnScreen(base::TimeTicks initial_time);

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_METRICS_UTIL_H_
