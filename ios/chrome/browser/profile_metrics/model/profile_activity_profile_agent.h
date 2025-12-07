// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_ACTIVITY_PROFILE_AGENT_H_
#define IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_ACTIVITY_PROFILE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

// An profile agent that marks the corresponding ProfileIOS (and, if it exists,
// its primary account) as active in the ProfileAttributesStorageIOS based on
// SceneActivationLevel changes.
@interface ProfileActivityProfileAgent : SceneObservingProfileAgent
@end

#endif  // IOS_CHROME_BROWSER_PROFILE_METRICS_MODEL_PROFILE_ACTIVITY_PROFILE_AGENT_H_
