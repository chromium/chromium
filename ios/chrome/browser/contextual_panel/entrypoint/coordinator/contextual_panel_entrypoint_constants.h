// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_CONSTANTS_H_

#import "base/time/time.h"

// How many seconds delay before the large Contextual Panel Entrypoint is shown
// (timer starts after the normal entrypoint is shown).
inline constexpr base::TimeDelta
    kLargeContextualPanelEntrypointAppearanceDelay = base::Seconds(2);

// How many seconds the large Contextual Panel Entrypoint is shown for, which
// includes disabling fullscreen.
inline constexpr base::TimeDelta
    kLargeContextualPanelEntrypointDisplayDuration = base::Seconds(4);

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_CONSTANTS_H_
