// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TEST_UTILS_H_

#import <Foundation/Foundation.h>

class PrefService;

inline constexpr char kDataControlsBlockedUrl[] = "https://block.com";

// Sets a Data Controls policy to block copying from `kDataControlsBlockedUrl`.
void SetCopyBlockRule(PrefService* prefs);

// Waits until a known pasteboard source is available from the pasteboard
// manager.
[[nodiscard]] bool WaitForKnownPasteboardSource();

// Waits until the pasteboard source is unknown.
[[nodiscard]] bool WaitForUnknownPasteboardSource();

// Waits until the pasteboard contains `expected_string`.
[[nodiscard]] bool WaitForStringInPasteboard(NSString* expected_string);

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TEST_UTILS_H_
