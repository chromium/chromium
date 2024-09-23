// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_UTILS_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_UTILS_H_

@class SceneState;

// Returns whether Promos Manager can display promos in current session (cold
// start to termination).
bool ShouldPromoManagerDisplayPromos();

// Returns YES if a promo can be displayed for the given scene state.
bool IsUIAvailableForPromo(SceneState* scene_state);

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_UTILS_H_
