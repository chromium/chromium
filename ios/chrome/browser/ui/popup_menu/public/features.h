// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

// Parameter name & value to choose which string to display to bookmark a page.
extern const char kPopupMenuBookmarkStringParamName[];
extern const char kPopupMenuBookmarkStringParamAddABookmark[];
extern const char kPopupMenuBookmarkStringParamAddToBookmarks[];
extern const char kPopupMenuBookmarkStringParamBookmarkThisPage[];

// Feature flag to change the string of the "bookmark" option in the overflow
// menu.
extern const base::Feature kBookmarkString;

// Returns the bookmark string ID based on the `kBookmarkString` feature.
int GetBookmarkStringID();

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_FEATURES_H_
