// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_STATE_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_STATE_AGENT_H_

#import "base/functional/callback.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

// A SceneStateObserver that is used by the PersistTabContextBrowserAgent to
// detect when the scene state changes, specifically for detecting app
// backgrounding events.
@interface PersistTabContextStateAgent : NSObject <SceneStateObserver>

// Initializes the agent with a callback to be run on scene activation level
// changes. `callback` will be executed with the new SceneActivationLevel.
- (instancetype)initWithTransitionCallback:
    (base::RepeatingCallback<void(SceneActivationLevel)>)callback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_STATE_AGENT_H_
