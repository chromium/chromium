// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_CONTENT_SUGGESTIONS_SCHEDULER_APP_STATE_AGENT_H_
#define IOS_CHROME_APP_CONTENT_SUGGESTIONS_SCHEDULER_APP_STATE_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// The agent that notifies the content suggestions service about app lifecycle
// events to keep the model up to date.
@interface ContentSuggestionsSchedulerAppAgent : SceneObservingAppAgent
@end

#endif  // IOS_CHROME_APP_CONTENT_SUGGESTIONS_SCHEDULER_APP_STATE_AGENT_H_
