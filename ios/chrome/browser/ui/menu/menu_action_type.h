// Copyright 2020 The Chromium Authors
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
  CopyURL = 4,
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
  SaveImage = 19,
  OpenImageInCurrentTab = 20,
  SearchImage = 21,
  CloseAllTabs = 22,
  SelectTabs = 23,
  OpenImageInNewTab = 24,
  CopyImage = 25,
  SearchImageWithLens = 26,
  ShowLinkPreview = 27,
  HideLinkPreview = 28,
  OpenNewTab = 29,
  OpenNewIncognitoTab = 30,
  CloseCurrentTabs = 31,
  ShowQRScanner = 32,
  StartVoiceSearch = 33,
  StartNewSearch = 34,
  StartNewIncognitoSearch = 35,
  SearchCopiedImage = 36,
  VisitCopiedLink = 37,
  SearchCopiedText = 38,
  PinTab = 39,
  UnpinTab = 40,
  LensCameraSearch = 41,
  SaveImageToGooglePhotos = 42,
  kMaxValue = SaveImageToGooglePhotos
};

#endif  // IOS_CHROME_BROWSER_UI_MENU_MENU_ACTION_TYPE_H_
