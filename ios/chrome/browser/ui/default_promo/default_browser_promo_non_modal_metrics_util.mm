// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_metrics_util.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;
using base::UmaHistogramEnumeration;

void LogNonModalPromoAction(NonModalPromoAction action,
                            NonModalPromoTriggerType type,
                            NSInteger impression_number) {
  switch (action) {
    case NonModalPromoAction::kAppear:
      RecordAction(
          UserMetricsAction("IOS.DefaultBrowserPromo.NonModal.Appear"));
      break;
    case NonModalPromoAction::kAccepted:
      RecordAction(
          UserMetricsAction("IOS.DefaultBrowserPromo.NonModal.Accepted"));
      break;
    case NonModalPromoAction::kDismiss:
      RecordAction(
          UserMetricsAction("IOS.DefaultBrowserPromo.NonModal.Dismiss"));
      break;
    case NonModalPromoAction::kTimeout:
      RecordAction(
          UserMetricsAction("IOS.DefaultBrowserPromo.NonModal.Timeout"));
      break;
    case NonModalPromoAction::kBackgroundCancel:
      // No-op.
      break;
    default:
      NOTREACHED();
      break;
  }

  if (impression_number == 0) {
    UmaHistogramEnumeration("IOS.DefaultBrowserPromo.NonModal.FirstImpression",
                            action);
  } else if (impression_number == 1) {
    UmaHistogramEnumeration("IOS.DefaultBrowserPromo.NonModal.SecondImpression",
                            action);
  }

  switch (type) {
    case NonModalPromoTriggerType::kPastedLink:
      UmaHistogramEnumeration(
          "IOS.DefaultBrowserPromo.NonModal.VisitPastedLink", action);

      break;
    case NonModalPromoTriggerType::kShare:
      UmaHistogramEnumeration("IOS.DefaultBrowserPromo.NonModal.Share", action);

      break;
    case NonModalPromoTriggerType::kGrowthKitOpen:
      UmaHistogramEnumeration("IOS.DefaultBrowserPromo.NonModal.GrowthKit",
                              action);

      break;
    default:
      NOTREACHED();
      break;
  }
}

void LogNonModalTimeOnScreen(base::TimeTicks initial_time) {
  if (initial_time.is_null()) {
    return;
  }
  UmaHistogramMediumTimes("IOS.DefaultBrowserPromo.NonModal.OnScreenTime",
                          base::TimeTicks::Now() - initial_time);
}
