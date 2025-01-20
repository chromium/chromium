// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_TESTING_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_TESTING_H_

#include "ios/chrome/app/application_delegate/app_state.h"

// Testing category exposing private methods of AppState for tests.
@interface AppState (Testing) <AppStateObserver>

// Redefined internally as readwrite.
@property(nonatomic, assign) AppInitStage initStage;

// Updates the value of `initStage` without any of the associated checks or
// logic.
- (void)updateInitStage:(AppInitStage)initStage;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_TESTING_H_
