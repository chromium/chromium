// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/profile_init_stage.h"

class ChromeBrowserState;

// Represents the state for a single Profile and responds to the state
// changes and system events.
@interface ProfileState : NSObject

// Profile initialisation stage.
@property(nonatomic, assign) ProfileInitStage initStage;

// The non-incognito ChromeBrowserState used for this Profile. This will be null
// until `initStage` >= `InitStageProfileLoaded`.
@property(nonatomic, assign) ChromeBrowserState* browserState;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
