// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_promo_non_modal_metrics_util.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"

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
      NOTREACHED_IN_MIGRATION();
      break;
  }

  std::string histogramString;

  switch (impression_number) {
    case 0:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.FirstImpression";
      break;

    case 1:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.SecondImpression";
      break;

    case 2:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.ThirdImpression";
      break;

    case 3:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.FourthImpression";
      break;

    case 4:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.FifthImpression";
      break;

    case 5:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.SixthImpression";
      break;

    case 6:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.SeventhImpression";
      break;

    case 7:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.EighthImpression";
      break;

    case 8:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.NinthImpression";
      break;

    case 9:
      histogramString = "IOS.DefaultBrowserPromo.NonModal.TenthImpression";
      break;

    default:
      // TODO(crbug.com/327429982): M124 validation necessary.
      NOTREACHED(base::NotFatalUntil::M126);
  }

  UmaHistogramEnumeration(histogramString, action);

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
      NOTREACHED_IN_MIGRATION();
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
