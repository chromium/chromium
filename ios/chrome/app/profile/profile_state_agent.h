// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_AGENT_H_

#import <Foundation/Foundation.h>

@class ProfileState;

// ProfileState agents are objects owned by the profile state and providing some
// profile-scoped function. They can be driven by ProfileStateObserver events.
@protocol ProfileStateAgent <NSObject>

@required
// Sets the associated profile state. Called once and only once. Consider
// starting the profile state observation in your implementation of this method.
// Do not call this method directly. Calling [ProfileState addAgent]: will call
// it.
- (void)setProfileState:(ProfileState*)profileState;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_AGENT_H_
