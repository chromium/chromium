// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_READING_LIST_BACKGROUND_SESSION_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_MAIN_READING_LIST_BACKGROUND_SESSION_SCENE_AGENT_H_

#import "ios/chrome/browser/ui/main/observing_scene_state_agent.h"

// A scene agent that resets an NSUserDefault property for the Add to Reading
// List Messages when a new browser session has been deemed to have started.
@interface ReadingListBackgroundSessionSceneAgent : ObservingSceneAgent
@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_READING_LIST_BACKGROUND_SESSION_SCENE_AGENT_H_
