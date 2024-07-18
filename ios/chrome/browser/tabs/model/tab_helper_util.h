// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_

#import <type_traits>

namespace web {
class WebState;
}

// A bitmask to filter the tab helpers attached in `AttachTabHelpers`.
enum class TabHelperFilter {
  // No filter.
  kEmpty = 0,
  // Filter out tab helpers that are not needed during prerendering.
  kPrerender = 1 << 0,
  // Filter out tab helpers that are not needed when the web state
  // is presented in a bottom sheet (BVC's toolbars are covered).
  kBottomSheet = 1 << 1,
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

// Attaches tab helpers to WebState. Filter the attached tab helpers with
// `filter_flags`. This function is idempotent, so it is okay to call it
// multiple times for the same WebState. When called with a different
// `filter_flags` value, the right thing (adding helpers that weren't added in
// the prior call) will happen, although tab helpers will never be removed.
void AttachTabHelpers(web::WebState* web_state,
                      TabHelperFilter filter_flags = TabHelperFilter::kEmpty);

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_HELPER_UTIL_H_
