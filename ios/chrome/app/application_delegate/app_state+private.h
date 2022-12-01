// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_PRIVATE_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_PRIVATE_H_

#include "ios/chrome/app/application_delegate/app_state.h"

@class SafeModeCoordinator;

// Class extension exposing private methods of AppState for testing.
@interface AppState () <AppStateObserver>

@property(nonatomic, retain) SafeModeCoordinator* safeModeCoordinator;

// Redefined internally as readwrite.
@property(nonatomic, assign) InitStage initStage;

- (void)queueTransitionToFirstInitStage;

- (void)completeUIInitialization;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_PRIVATE_H_
