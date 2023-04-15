// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_INCOGNITO_BLOCKER_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_MAIN_INCOGNITO_BLOCKER_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

// A scene agent that shows a UI overlay to prevent incognito content from being
// shown in the task switcher.
@interface IncognitoBlockerSceneAgent : NSObject <SceneAgent>
@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_INCOGNITO_BLOCKER_SCENE_AGENT_H_
