// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_UTIL_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_UTIL_H_

#import <optional>

#import "base/time/time.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

// Returns the time the most recent tab was opened for `scene_state`.
std::optional<base::Time> GetTimeMostRecentTabWasOpenForSceneState(
    SceneState* scene_state);

// Returns how much time has elapsed since the most recent tab was opened for
// `scene_state`.
std::optional<base::TimeDelta> GetTimeSinceMostRecentTabWasOpenForSceneState(
    SceneState* sceneState);

// Checks whether the tab group in grid should be shown for the given scene
// state.
bool ShouldShowTabGroupInGridForSceneState(SceneState* scene_state);

// Checks whether the Start Surface should be shown for the given scene state.
bool ShouldShowStartSurfaceForSceneState(SceneState* scene_state);

// Returns the string label containing the time since the most recent tab was
// open. Will return empty string if not applicable.
NSString* GetRecentTabTileTimeLabelForSceneState(SceneState* scene_state);

// Sets the session related objects for the Start Surface.
void SetStartSurfaceSessionObjectForSceneState(SceneState* scene_state);

namespace test {

// Sets the session related objects for the Start Surface in tests.
void SetStartSurfaceSessionObjectForSceneStateForTesting(
    SceneState* scene_state,
    base::Time timestamp);

}  // namespace test

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_UTIL_H_
