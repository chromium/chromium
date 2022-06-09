// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_BROWSER_AGENT_H_

#import "ios/chrome/browser/main/browser_user_data.h"

@class SceneState;

// Browser agent to associate a SceneState (and potentially other related
// objects) with a Browser.
class SceneStateBrowserAgent : public BrowserUserData<SceneStateBrowserAgent> {
 public:
  // Creates the browser agent, attaching it to |browser| and associating
  // |scene_state| with it.
  static void CreateForBrowser(Browser* browser, SceneState* scene_state);

  // Not copyable or moveable
  SceneStateBrowserAgent(const SceneStateBrowserAgent&) = delete;
  SceneStateBrowserAgent& operator=(const SceneStateBrowserAgent&) = delete;
  ~SceneStateBrowserAgent() override;

  // Returns the SceneState associated with the browser
  SceneState* GetSceneState();

 private:
  friend class BrowserUserData<SceneStateBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  SceneStateBrowserAgent(Browser* browser, SceneState* scene_state);

  // The associated SceneState.
  __weak SceneState* scene_state_;
};

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_BROWSER_AGENT_H_
