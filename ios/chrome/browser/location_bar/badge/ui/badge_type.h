// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_BADGE_TYPE_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_BADGE_TYPE_H_

// Features that can be displayed as a badge in the location bar.
enum class BadgeType {
  kNone = 0,
  kBadgeView,
  kIncognito,
  kContextualPanel,
  kReaderMode,
};

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_BADGE_TYPE_H_
