// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_METRICS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_METRICS_H_

namespace signin {

// Histogram recording lifecycle events for the Smart Account Switching startup
// promo.
inline constexpr char kAccountSwitchingPromoEventsHistogram[] =
    "Signin.AccountSwitchingPromo.Events";

// Events recorded to the `Signin.AccountSwitchingPromo.Events` histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AccountSwitchingPromoEvent)
enum class AccountSwitchingPromoEvent {
  // Emitted at startup by `AccountSwitchingPromoSceneAgent` when the promo is
  // eligible and registered with `PromosManager`.
  kEligibleOnStartup = 0,

  kMaxValue = kEligibleOnStartup,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountSwitchingPromoEvent)

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_METRICS_H_
