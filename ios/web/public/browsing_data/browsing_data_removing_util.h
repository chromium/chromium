// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSING_DATA_BROWSING_DATA_REMOVING_UTIL_H_
#define IOS_WEB_PUBLIC_BROWSING_DATA_BROWSING_DATA_REMOVING_UTIL_H_

#include <type_traits>

#include "base/callback.h"
#include "base/time/time.h"

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

};

// Clears the browsing data store in the Web layer. |modified_since| is the data
// since which all data is removed. |closure| is called when the browsing data
// have been cleared.
// TODO(crbug.com/906199): Remove closure once WebStateObserver callback is
// implemented.
void ClearBrowsingData(BrowserState* browser_state,
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

// Returns whether the |flag| is set in |mask|.
constexpr bool IsRemoveDataMaskSet(ClearBrowsingDataMask mask,
                                   ClearBrowsingDataMask flag) {
  return (mask & flag) == flag;
}

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSING_DATA_BROWSING_DATA_REMOVING_UTIL_H_
