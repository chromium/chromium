// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_RESTORE_METRICS_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_RESTORE_METRICS_H_

namespace post_restore_default_browser {

// Name of the histogram that logs the action taken by the user after the Post
// Restore Default Browser promo is presented.
extern const char kPromptActionHistogramName[];

// Name of the user action that logs when Post Restore Default Browser promo
// has been displayed.
extern const char kPromptDisplayedUserActionName[];

// Interactions with the initial Post Restore Default Browser promo. This is
// mapped to the IOSPostRestoreDefaultBrowserActionOnPrompt enum in enums.xml
// for metrics.
enum class PromptActionType {
  kNoThanks = 0,
  kGoToSettings = 1,
  kSwipeToDismiss = 2,
  kMaxValue = kSwipeToDismiss,
};

}  // namespace post_restore_default_browser

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_RESTORE_METRICS_H_
