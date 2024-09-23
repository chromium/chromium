// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_METRICS_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_METRICS_H_

namespace post_default_abandonment {

// Enum for the user action when the prompt is shown. This reuses the
// IOSPostRestoreDefaultBrowserActionOnPrompt enum in enums.xml instead of
// introducing a similar enum.
enum class UserActionType {
  kNoThanks = 0,
  kGoToSettings = 1,
  kMaxValue = kGoToSettings,
};

// Records the user action resulting from the promo.
void RecordPostDefaultAbandonmentPromoUserAction(UserActionType action);

}  // namespace post_default_abandonment

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_METRICS_H_
