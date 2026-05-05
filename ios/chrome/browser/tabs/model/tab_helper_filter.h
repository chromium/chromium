// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_FILTER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_FILTER_H_

#import <type_traits>

// A bitmask to filter the tab helpers attached in `AttachTabHelpers`.
enum class TabHelperFilter {
  // No filter.
  kEmpty = 0,
  // Filter out tab helpers that are not needed during prerendering.
  kPrerender = 1 << 0,
  // Filter out tab helpers that are not needed when the web state
  // is presented in the lens overlay (BVC's toolbars are covered).
  kLensOverlay = 1 << 1,
  // Filter out tab helpers that are not needed when the web state
  // is presented for Reader Mode.
  kReaderMode = 1 << 2,
  // Filter out tab helpers that are not needed when the web state
  // is presented in the Assistant AIM sheet.
  // TODO(crbug.com/445918427): Define specific tab helpers to filter for
  // Assistant AIM.
  kAssistantAim = 1 << 3,
};

// Implementation of bitwise "or", "and" operators (as those are not
// automatically defined for "class enum").
constexpr TabHelperFilter operator|(TabHelperFilter lhs, TabHelperFilter rhs) {
  return static_cast<TabHelperFilter>(
      static_cast<std::underlying_type<TabHelperFilter>::type>(lhs) |
      static_cast<std::underlying_type<TabHelperFilter>::type>(rhs));
}

constexpr TabHelperFilter operator&(TabHelperFilter lhs, TabHelperFilter rhs) {
  return static_cast<TabHelperFilter>(
      static_cast<std::underlying_type<TabHelperFilter>::type>(lhs) &
      static_cast<std::underlying_type<TabHelperFilter>::type>(rhs));
}

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_FILTER_H_
