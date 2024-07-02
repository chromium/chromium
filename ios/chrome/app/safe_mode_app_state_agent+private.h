// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SAFE_MODE_APP_STATE_AGENT_PRIVATE_H_
#define IOS_CHROME_APP_SAFE_MODE_APP_STATE_AGENT_PRIVATE_H_

#include "ios/chrome/app/application_delegate/app_state_observer.h"
#include "ios/chrome/app/safe_mode_app_state_agent.h"
#include "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"
#include "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

// Class extension exposing private methods of SafeModeAppAgent for testing.
@interface SafeModeAppAgent () <AppStateObserver,
                                SafeModeCoordinatorDelegate,
                                SceneStateObserver>

// This flag is set when the first scene has activated since the startup, and
// never reset.
@property(nonatomic, assign) BOOL firstSceneHasActivated;

// Safe mode coordinator. If this is non-nil, the app is displaying the safe
// mode UI.
@property(nonatomic, strong) SafeModeCoordinator* safeModeCoordinator;

@end

#endif  // IOS_CHROME_APP_SAFE_MODE_APP_STATE_AGENT_PRIVATE_H_
