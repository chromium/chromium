// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_KEYBOARD_LOCK_BROWSERTEST_H_
#define CONTENT_BROWSER_KEYBOARD_LOCK_BROWSERTEST_H_

namespace content {

void SetWindowFocusForKeyboardLockBrowserTests(bool is_focused);

void InstallCreateHooksForKeyboardLockBrowserTests();

}  // namespace content

#endif  // CONTENT_BROWSER_KEYBOARD_LOCK_BROWSERTEST_H_
