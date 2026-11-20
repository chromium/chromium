// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_UTILS_H_

class Browser;
@class SceneState;

// Returns the regular Browser of the scene represented by `scene_state`.
// `scene_state` must be non-null and its scene must have a regular Browser;
// violating these preconditions is a CHECK failure.
Browser* GetRegularBrowserForScene(SceneState* scene_state);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_UTILS_H_
