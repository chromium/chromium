// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEYBOARD_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_KEYBOARD_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable the Keyboard Shortcuts Menu feature.
BASE_DECLARE_FEATURE(kKeyboardShortcutsMenu);

// Returns true if the Keyboard Shortcuts Menu feature is enabled.
bool IsKeyboardShortcutsMenuEnabled();

#endif  // IOS_CHROME_BROWSER_UI_KEYBOARD_FEATURES_H_
