// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVE_MASK_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVE_MASK_H_

#include <type_traits>

// Mask used to control which data to remove when clearing browsing
// data.
enum class BrowsingDataRemoveMask {
  REMOVE_NOTHING = 0,

  REMOVE_APPCACHE = 1 << 0,
  REMOVE_CACHE = 1 << 1,
  REMOVE_COOKIES = 1 << 2,
  REMOVE_DOWNLOADS = 1 << 3,
  REMOVE_FORM_DATA = 1 << 4,
  REMOVE_HISTORY = 1 << 5,
  REMOVE_INDEXEDDB = 1 << 6,
  REMOVE_LOCAL_STORAGE = 1 << 7,
  REMOVE_PASSWORDS = 1 << 8,
  REMOVE_WEBSQL = 1 << 9,
  REMOVE_CACHE_STORAGE = 1 << 11,
  REMOVE_VISITED_LINKS = 1 << 12,
  REMOVE_BOOKMARKS = 1 << 13,
  REMOVE_READING_LIST = 1 << 14,
  REMOVE_LAST_USER_ACCOUNT = 1 << 15,
  CLOSE_TABS = 1 << 16,

  // "Site data" includes cookies, appcache, indexed DBs, local storage, webSQL,
  // cache storage, and visited links.
  REMOVE_SITE_DATA = REMOVE_APPCACHE | REMOVE_COOKIES | REMOVE_INDEXEDDB |
                     REMOVE_LOCAL_STORAGE | REMOVE_CACHE_STORAGE |
                     REMOVE_WEBSQL | REMOVE_VISITED_LINKS,

  // Includes all the available remove options. Meant to be used by clients that
  // wish to wipe as much data as possible from a ProfileIOS, to make it
  // look like a new ProfileIOS. Does not include closing tabs as tabs
  // should only be closed by explicit user action.
  REMOVE_ALL = REMOVE_SITE_DATA | REMOVE_CACHE | REMOVE_DOWNLOADS |
               REMOVE_FORM_DATA | REMOVE_HISTORY | REMOVE_PASSWORDS |
               REMOVE_BOOKMARKS | REMOVE_READING_LIST |
               REMOVE_LAST_USER_ACCOUNT,

  // Includes all the available remove options that support partial deletion.
  // Does not include closing tabs as tabs should only be closed by explicit
  // user action.
  REMOVE_ALL_FOR_TIME_PERIOD = REMOVE_SITE_DATA | REMOVE_CACHE |
                               REMOVE_FORM_DATA | REMOVE_HISTORY |
                               REMOVE_PASSWORDS | REMOVE_LAST_USER_ACCOUNT,
};

// Implementation of bitwise "or", "and" operators and the corresponding
// assignment operators too (as those are not automatically defined for
// "class enum").
constexpr BrowsingDataRemoveMask operator|(BrowsingDataRemoveMask lhs,
                                           BrowsingDataRemoveMask rhs) {
  return static_cast<BrowsingDataRemoveMask>(
      static_cast<std::underlying_type<BrowsingDataRemoveMask>::type>(lhs) |
      static_cast<std::underlying_type<BrowsingDataRemoveMask>::type>(rhs));
}

constexpr BrowsingDataRemoveMask operator&(BrowsingDataRemoveMask lhs,
                                           BrowsingDataRemoveMask rhs) {
  return static_cast<BrowsingDataRemoveMask>(
      static_cast<std::underlying_type<BrowsingDataRemoveMask>::type>(lhs) &
      static_cast<std::underlying_type<BrowsingDataRemoveMask>::type>(rhs));
}

inline BrowsingDataRemoveMask& operator|=(BrowsingDataRemoveMask& lhs,
                                          BrowsingDataRemoveMask rhs) {
  lhs = lhs | rhs;
  return lhs;
}

inline BrowsingDataRemoveMask& operator&=(BrowsingDataRemoveMask& lhs,
                                          BrowsingDataRemoveMask rhs) {
  lhs = lhs & rhs;
  return lhs;
}

// Returns whether the `flag` is set in `mask`.
constexpr bool IsRemoveDataMaskSet(BrowsingDataRemoveMask mask,
                                   BrowsingDataRemoveMask flag) {
  return (mask & flag) == flag;
}

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVE_MASK_H_
