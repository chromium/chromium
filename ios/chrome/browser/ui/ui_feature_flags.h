// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Avoid the crash in https://crbug.com/816427 by getting the first responder by
// navigating the key window, rather than using -sendAction to find the first
// responder.
extern const base::Feature kFirstResponderKeyWindow;

// Feature to automatically switch to the regular tabs panel in tab grid after
// closing the last incognito tab.
extern const base::Feature kClosingLastIncognitoTab;

// Feature to contain the NTP directly from browser container.
extern const base::Feature kBrowserContainerContainsNTP;

// Feature to copy image to system pasteboard via context menu.
extern const base::Feature kCopyImage;

// Feature to show most visited sites and collection shortcuts in the omnibox
// popup instead of ZeroSuggest.
extern const base::Feature kOmniboxPopupShortcutIconsInZeroState;

// Used to enable using the WKWebView snapshotting API for iOS 11+.
extern const base::Feature kWKWebViewSnapshots;

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
