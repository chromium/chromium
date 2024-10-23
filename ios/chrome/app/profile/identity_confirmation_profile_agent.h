// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_IDENTITY_CONFIRMATION_PROFILE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_IDENTITY_CONFIRMATION_PROFILE_AGENT_H_

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

// Profile agent that triggers the signed-in identity confirmation when
// necessary.
@interface IdentityConfirmationProfileAgent : SceneObservingProfileAgent

@end

#endif  // IOS_CHROME_APP_PROFILE_IDENTITY_CONFIRMATION_PROFILE_AGENT_H_
