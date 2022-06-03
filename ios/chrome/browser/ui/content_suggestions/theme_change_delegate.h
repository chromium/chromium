// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_THEME_CHANGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_THEME_CHANGE_DELEGATE_H_

// Delegate for handling when the user's phone changes theme (light/dark).
@protocol ThemeChangeDelegate

// Handles any theme change, regardless of what the new theme is.
- (void)handleThemeChange;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_THEME_CHANGE_DELEGATE_H_
