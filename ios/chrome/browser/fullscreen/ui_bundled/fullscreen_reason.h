// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_REASON_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_REASON_H_

// The reason why the fullscreen mode was exited.
enum class FullscreenExitReason {
  kUserControlled,
  kForcedByCode,
  kUserInitiatedFinishedByCode,
  kUserTapped,
  kBottomReached,
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_REASON_H_
