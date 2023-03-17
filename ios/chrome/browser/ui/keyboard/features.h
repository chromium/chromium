// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEYBOARD_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_KEYBOARD_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable the Keyboard Shortcuts Menu feature.
// Use IsKeyboardShortcutsMenuEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kKeyboardShortcutsMenu);

// Whether the Keyboard Shortcuts Menu is enabled. Returns the value in
// NSUserDefaults set by `SaveFeedBackgroundRefreshEnabledForNextColdStart()`.
bool IsKeyboardShortcutsMenuEnabled();

// Saves the current value for feature `kKeyboardShortcutsMenu`. This call
// DCHECKs on the availability of `base::FeatureList`.
void SaveKeyboardShortcutsMenuEnabledForNextColdStart();

#endif  // IOS_CHROME_BROWSER_UI_KEYBOARD_FEATURES_H_
