// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGES_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGES_HISTOGRAMS_H_

// Histogram name for tapping on a row in the Badge overflow menu.
extern const char kInfobarOverflowMenuTappedHistogram[];

// Values for the Mobile.Messages.OverflowRow.Tapped histogram. Entries should
// not be renumbered and numeric values should never be reused.
// Please also update MobileMessagesInfobarType enum in
// tools/metrics/histograms/enums.xml
enum class MobileMessagesInfobarType {
  Confirm = 0,
  SavePassword = 1,
  UpdatePassword = 2,
  SaveCard = 3,
  Translate = 4,
  Permissions = 5,
  AutofillSaveAddressProfile = 6,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = AutofillSaveAddressProfile,
};

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGES_HISTOGRAMS_H_
