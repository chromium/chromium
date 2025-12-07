// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSING_DATA_BROWSING_DATA_REMOVING_UTIL_H_
#define IOS_WEB_PUBLIC_BROWSING_DATA_BROWSING_DATA_REMOVING_UTIL_H_

#include <type_traits>

#include "base/functional/callback.h"
#include "base/time/time.h"

@class UIWindow;

namespace web {

class BrowserState;

// Mask used to control which data to remove when clearing browsing
// data.
enum class ClearBrowsingDataMask {
  kRemoveNothing = 0,

  kRemoveAppCache = 1 << 0,
  kRemoveCookies = 1 << 1,
  kRemoveIndexedDB = 1 << 2,
  kRemoveLocalStorage = 1 << 3,
  kRemoveWebSQL = 1 << 4,
  kRemoveCacheStorage = 1 << 5,
  kRemoveVisitedLinks = 1 << 6,
  kRemoveOriginPrivateFileSystem = 1 << 7,
  kRemoveServiceWorkers = 1 << 8,
};

// Clears the browsing data store in the Web layer. `modified_since` is the data
// since which all data is removed. `closure` is called when the browsing data
// have been cleared.
//
// `window` is used to insert the WKWebView in the view hierarchy to ensure the
// out-of-process process used by the WKWebView is not suspended (the process
// appear to be sometimes suspended when the WKWebView is not part of the view
// hierarchy according to the unit tests).
//
// TODO(crbug.com/40602822): Remove closure once WebStateObserver callback is
// implemented.
void ClearBrowsingData(UIWindow* window,
                       BrowserState* browser_state,
                       ClearBrowsingDataMask types,
                       base::Time modified_since,
                       base::OnceClosure closure);

// Implementation of bitwise "or", "and" operators and the corresponding
// assignment operators too (as those are not automatically defined for
// "class enum").
constexpr ClearBrowsingDataMask operator|(ClearBrowsingDataMask lhs,
                                          ClearBrowsingDataMask rhs) {
  return static_cast<ClearBrowsingDataMask>(
      static_cast<std::underlying_type<ClearBrowsingDataMask>::type>(lhs) |
      static_cast<std::underlying_type<ClearBrowsingDataMask>::type>(rhs));
}

constexpr ClearBrowsingDataMask operator&(ClearBrowsingDataMask lhs,
                                          ClearBrowsingDataMask rhs) {
  return static_cast<ClearBrowsingDataMask>(
      static_cast<std::underlying_type<ClearBrowsingDataMask>::type>(lhs) &
      static_cast<std::underlying_type<ClearBrowsingDataMask>::type>(rhs));
}

inline ClearBrowsingDataMask& operator|=(ClearBrowsingDataMask& lhs,
                                         ClearBrowsingDataMask rhs) {
  lhs = lhs | rhs;
  return lhs;
}

inline ClearBrowsingDataMask& operator&=(ClearBrowsingDataMask& lhs,
                                         ClearBrowsingDataMask rhs) {
  lhs = lhs & rhs;
  return lhs;
}

// Returns whether the `flag` is set in `mask`.
constexpr bool IsRemoveDataMaskSet(ClearBrowsingDataMask mask,
                                   ClearBrowsingDataMask flag) {
  return (mask & flag) == flag;
}

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSING_DATA_BROWSING_DATA_REMOVING_UTIL_H_
