// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_PUBLIC_CONTEXTUAL_DEFAULT_BROWSER_PROMO_METRICS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_PUBLIC_CONTEXTUAL_DEFAULT_BROWSER_PROMO_METRICS_H_

enum class ContextualDefaultBrowserPromoType;

// Enum actions for the IOS.DefaultBrowserContextualPromo.* UMA metrics.
// LINT.IfChange(IOSDefaultBrowserContextualPromoAction)
enum class ContextualDefaultBrowserPromoAction {
  kPrimaryActionTapped = 0,
  kSecondaryActionTapped = 1,
  kSwipeDown = 2,
  kMaxValue = kSwipeDown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSDefaultBrowserContextualPromoAction)

// Records that a contextual default browser promo was shown.
void RecordContextualDefaultBrowserPromoShown(
    ContextualDefaultBrowserPromoType promo_type);

// Records user action taken on a contextual default browser promo.
void RecordContextualDefaultBrowserPromoAction(
    ContextualDefaultBrowserPromoType promo_type,
    ContextualDefaultBrowserPromoAction action);

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_CONTEXTUAL_PUBLIC_CONTEXTUAL_DEFAULT_BROWSER_PROMO_METRICS_H_
