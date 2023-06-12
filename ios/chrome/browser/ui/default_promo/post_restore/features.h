// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_POST_RESTORE_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_POST_RESTORE_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable the post restore default browser promo.
BASE_DECLARE_FEATURE(kPostRestoreDefaultBrowserPromo);

// Enum for "Post Restore Default Browser" experiment groups.
enum class PostRestoreDefaultBrowserPromoType {
  // "Post Restore Default Browser" enabled with an alert style promo.
  kAlert = 0,
  // "Post Restore Default Browser" enabled with a half sheet promo.
  kHalfscreen = 1,
  // "Post Restore Default Browser" enabled with a full screen promo.
  kFullscreen = 2,
  // "Post Restore Default Browser" not enabled.
  kDisabled,
};

// Feature param for the halfscreen promo.
extern const char kPostRestoreDefaultBrowserPromoHalfscreenParam[];
// Feature param for the fullscreen promo.
extern const char kPostRestoreDefaultBrowserPromoFullscreenParam[];

// Returns the current PostRestoreDefaultBrowserPromoType according to the
// feature flag and experiment "PostRestoreDefaultBrowserPromoIOS".
PostRestoreDefaultBrowserPromoType GetPostRestoreDefaultBrowserPromoType();

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_POST_RESTORE_FEATURES_H_
