// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_H_
#define IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

class PrefRegistrySimple;

// The agent that manages the transition for AppInitStage::kVariationsSeed. See
// comment in AppInitStage enum definition for more information.
@interface VariationsAppStateAgent : SceneObservingAppAgent

// Registers the prefs required for this agent.
+ (void)registerLocalState:(PrefRegistrySimple*)registry;

@end

#endif  // IOS_CHROME_APP_VARIATIONS_APP_STATE_AGENT_H_
