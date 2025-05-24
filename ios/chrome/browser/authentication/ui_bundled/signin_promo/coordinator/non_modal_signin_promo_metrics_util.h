// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_METRICS_UTIL_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_METRICS_UTIL_H_

#import "base/time/time.h"

enum class SignInPromoType;

// Possible actions for a non-modal sign-in promo.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(NonModalSignInPromoAction)
enum class NonModalSignInPromoAction {
  kAccept = 0,   // User tapped the sign-in button.
  kAppear = 1,   // Promo was shown to the user.
  kDismiss = 2,  // User dismissed the promo.
  kTimeout = 3,  // Promo automatically timed out.
  kMaxValue = kTimeout
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:NonModalSignInPromoAction)

// Logs metrics for an action happening in a non-modal sign-in promo.
void LogNonModalSignInPromoAction(NonModalSignInPromoAction action,
                                  SignInPromoType promo_type);

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_COORDINATOR_NON_MODAL_SIGNIN_PROMO_METRICS_UTIL_H_
