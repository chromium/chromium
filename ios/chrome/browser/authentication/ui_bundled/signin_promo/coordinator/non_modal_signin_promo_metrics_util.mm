// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_metrics_util.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/signin_promo_types.h"

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;

void LogNonModalSignInPromoAction(NonModalSignInPromoAction action,
                                  SignInPromoType promo_type) {
  // Record metrics specific to the promo type.
  switch (promo_type) {
    case SignInPromoType::kPassword:
      UmaHistogramEnumeration("IOS.SignInPromo.NonModal.Password", action);
      break;
    case SignInPromoType::kBookmark:
      UmaHistogramEnumeration("IOS.SignInPromo.NonModal.Bookmark", action);
      break;
  }
}
