// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_ACTIVITY_APP_AGENT_H_
#define IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_ACTIVITY_APP_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// An app agent that marks the corresponding ProfileIOS (and, if it exists, its
// primary account) as active in the ProfileAttributesStorageIOS based on
// SceneActivationLevel changes.
//
// Note that this observes both AppState and SceneState, so that it can detect
// scene activations even if they happen before the corresponding ProfileIOS has
// been loaded.
@interface ProfileActivityAppAgent : SceneObservingAppAgent
@end

#endif  // IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_ACTIVITY_APP_AGENT_H_
