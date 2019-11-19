// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_MULTI_WINDOW_SUPPORT_H_
#define IOS_CHROME_BROWSER_UI_UTIL_MULTI_WINDOW_SUPPORT_H_

// Returns true if multiwindow is supported on this OS version and is enabled in
// the current build configuration. Does not check if this device can actually
// show multiple windows (e.g. on iPhone): use [UIApplication
// supportsMultipleScenes] instead.
bool IsMultiwindowSupported();

#endif  // IOS_CHROME_BROWSER_UI_UTIL_MULTI_WINDOW_SUPPORT_H_
