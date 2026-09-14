// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/contextual/public/contextual_default_browser_promo_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/promo/contextual/public/contextual_default_browser_promo_constants.h"

namespace {

// Returns the histogram name for a given promo type action.
const char* ActionHistogramNameForPromoType(
    ContextualDefaultBrowserPromoType promo_type) {
  switch (promo_type) {
    case ContextualDefaultBrowserPromoType::kGemini:
      return "IOS.DefaultBrowserContextualPromo.Gemini.Action";
  }
  NOTREACHED();
}

}  // namespace

void RecordContextualDefaultBrowserPromoShown(
    ContextualDefaultBrowserPromoType promo_type) {
  base::UmaHistogramEnumeration("IOS.DefaultBrowserContextualPromo.Shown",
                                promo_type);
  switch (promo_type) {
    case ContextualDefaultBrowserPromoType::kGemini:
      base::RecordAction(base::UserMetricsAction(
          "IOS.DefaultBrowserContextualPromo.Gemini.Impression"));
      break;
  }
}

void RecordContextualDefaultBrowserPromoAction(
    ContextualDefaultBrowserPromoType promo_type,
    ContextualDefaultBrowserPromoAction action) {
  base::UmaHistogramEnumeration(ActionHistogramNameForPromoType(promo_type),
                                action);

  switch (action) {
    case ContextualDefaultBrowserPromoAction::kPrimaryActionTapped:
      RecordDefaultBrowserPromoLastAction(
          IOSDefaultBrowserPromoAction::kActionButton);
      switch (promo_type) {
        case ContextualDefaultBrowserPromoType::kGemini:
          base::RecordAction(base::UserMetricsAction(
              "IOS.DefaultBrowserContextualPromo.Gemini.OpenSettingsTapped"));
          break;
      }
      break;
    case ContextualDefaultBrowserPromoAction::kSecondaryActionTapped:
      RecordDefaultBrowserPromoLastAction(
          IOSDefaultBrowserPromoAction::kCancel);
      switch (promo_type) {
        case ContextualDefaultBrowserPromoType::kGemini:
          base::RecordAction(base::UserMetricsAction(
              "IOS.DefaultBrowserContextualPromo.Gemini.Dismiss"));
          break;
      }
      break;
    case ContextualDefaultBrowserPromoAction::kSwipeDown:
      RecordDefaultBrowserPromoLastAction(
          IOSDefaultBrowserPromoAction::kDismiss);
      switch (promo_type) {
        case ContextualDefaultBrowserPromoType::kGemini:
          base::RecordAction(base::UserMetricsAction(
              "IOS.DefaultBrowserContextualPromo.Gemini.Dismiss"));
          break;
      }
      break;
  }
}
