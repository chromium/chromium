// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_OPTIONS_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_OPTIONS_H_

#include <string>

@class ProfileState;

// Options passed when connecting a SceneState.
struct SceneStateOptions {
  ProfileState* profile_state;
  std::string identifier;
};

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_OPTIONS_H_
