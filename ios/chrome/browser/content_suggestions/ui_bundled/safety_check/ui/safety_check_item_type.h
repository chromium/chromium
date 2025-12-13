// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_UI_SAFETY_CHECK_ITEM_TYPE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_UI_SAFETY_CHECK_ITEM_TYPE_H_

// The possible types of Safety Check (Magic Stack) module items.
enum class SafetyCheckItemType {
  kAllSafe = 1,
  kRunning = 2,
  kUpdateChrome = 3,
  kPassword = 4,
  kSafeBrowsing = 5,
  kDefault = 6,
  kMaxValue = kDefault
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_UI_SAFETY_CHECK_ITEM_TYPE_H_
