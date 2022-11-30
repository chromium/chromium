// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_ENTERPRISE_APP_AGENT_H_
#define IOS_CHROME_APP_ENTERPRISE_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/app_state_agent.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"

// App state agent that blocks the app initialization while the
// enterprise-related data is loaded.
@interface EnterpriseAppAgent : NSObject <AppStateAgent, AppStateObserver>

@end

#endif  // IOS_CHROME_APP_ENTERPRISE_APP_AGENT_H_
