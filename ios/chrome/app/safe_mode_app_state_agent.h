// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SAFE_MODE_APP_STATE_AGENT_H_
#define IOS_CHROME_APP_SAFE_MODE_APP_STATE_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// App state agent that triggers the safe mode stage when needed.
@interface SafeModeAppAgent : SceneObservingAppAgent
@end

#endif  // IOS_CHROME_APP_SAFE_MODE_APP_STATE_AGENT_H_
