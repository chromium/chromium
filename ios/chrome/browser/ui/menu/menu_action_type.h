// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_MENU_ACTION_TYPE_H_
#define IOS_CHROME_BROWSER_UI_MENU_MENU_ACTION_TYPE_H_

// Enum representing the existing set of menu actions as types. Current values
// should not be renumbered. Please keep in sync with "IOSMenuAction" in
// src/tools/metrics/histograms/enums.xml.
enum class MenuActionType {
  OpenInNewTab = 0,
  OpenInNewIncognitoTab = 1,
  OpenInNewWindow = 2,
  OpenAllInNewTabs = 3,
  Copy = 4,
  Edit = 5,
  Move = 6,
  Share = 7,
  Delete = 8,
  Remove = 9,
  Hide = 10,
  Read = 11,
  Unread = 12,
  ViewOffline = 13,
  OpenJavascript = 14,
  AddToReadingList = 15,
  AddToBookmarks = 16,
  CloseTab = 17,
  EditBookmark = 18,
  Save = 19,
  OpenInCurrentTab = 20,
  SearchImage = 21,
  kMaxValue = SearchImage
};

#endif  // IOS_CHROME_BROWSER_UI_MENU_MENU_ACTION_TYPE_H_
