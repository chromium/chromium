// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_ACCESSIBILITY_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_ACCESSIBILITY_UTIL_H_

#include "base/time/time.h"

// Time duration to wait before auto-dismissing a confirmation state when
// VoiceOver is off.
inline constexpr base::TimeDelta kConfirmationDismissDelay = base::Seconds(1.5);

// Time duration to wait before auto-dismissing a confirmation state when
// VoiceOver is running, giving VoiceOver enough time to read the announcement.
// TODO(crbug.com/413009143): Listen for the VoiceOver announcement to finish
// instead of relying solely on a fixed delay.
inline constexpr base::TimeDelta kConfirmationDismissDelayIfVoiceOverRunning =
    base::Seconds(5);

// Returns the delay before auto-dismissing a confirmation state:
// `kConfirmationDismissDelayIfVoiceOverRunning` when VoiceOver is running,
// `kConfirmationDismissDelay` otherwise.
base::TimeDelta GetConfirmationDismissDelay();

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_ACCESSIBILITY_UTIL_H_
