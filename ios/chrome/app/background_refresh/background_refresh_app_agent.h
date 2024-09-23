// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

@class AppRefreshProvider;

// An app agent that manages background refresh tasks.
// DEBUGGING/TESTING note: App refresh does not work on simulators; you must
// use a device.
@interface BackgroundRefreshAppAgent : SceneObservingAppAgent

// Register `provider` as providing app refresh tasks. Registration must happen
// before refresh tasks execute, ideally during basic app init.
- (void)addAppRefreshProvider:(AppRefreshProvider*)provider;

// TODO(crbug.com/354918794): Add an API for removing a provider if needed.

// TODO(crbug.com/354918794): Add an API for recurring refreshes, or make that
// the default.

// TODO(crbug.com/354918794): Add an API for cancelling any pending refresh, if
// needed.

@end

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_H_
