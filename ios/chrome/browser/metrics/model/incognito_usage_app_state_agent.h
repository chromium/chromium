// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_INCOGNITO_USAGE_APP_STATE_AGENT_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_INCOGNITO_USAGE_APP_STATE_AGENT_H_

#import "ios/chrome/app/application_delegate/app_state_agent.h"

// The agent that logs the length of continuous usage of incognito.
// Any normal/incognito transition lasting less than 10 seconds will be ignored.
@interface IncognitoUsageAppStateAgent : NSObject <AppStateAgent>
@end

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_INCOGNITO_USAGE_APP_STATE_AGENT_H_
