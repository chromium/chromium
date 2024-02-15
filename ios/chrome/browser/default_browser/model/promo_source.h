// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_

// An histogram to report the source of the default browser promo.
// Used for UMA, do not reorder.
// LINT.IfChange
enum class DefaultBrowserPromoSource {
  kSettings = 0,
  kOmnibox = 1,
  kExternalIntent = 2,
  kSetUpList = 3,
  // kExternalAction refers to Chrome being opened with a "ChromeExternalAction"
  // host.
  kExternalAction = 4,
  kTipsNotification = 5,
  kMaxValue = kTipsNotification,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml)

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_
