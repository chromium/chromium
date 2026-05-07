// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOW_TO_CHROME_MODEL_HOW_TO_CHROME_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_HOW_TO_CHROME_MODEL_HOW_TO_CHROME_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

// Manager for the "How to Chrome" feature.
// Listens to user actions and marks tasks as completed.
@interface HowToChromeSceneAgent : ObservingSceneAgent

// Starts listening to user actions.
- (void)startListening;

// Stops listening to user actions.
- (void)stopListening;

@end

#endif  // IOS_CHROME_BROWSER_HOW_TO_CHROME_MODEL_HOW_TO_CHROME_SCENE_AGENT_H_
