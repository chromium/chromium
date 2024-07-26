// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_CONTROLLER_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_CONTROLLER_H_

#import <Foundation/Foundation.h>

@class ProfileState;

// The controller for a single Profile, owned by MainController. Owns all
// the top-level UI controllers for this Profile.
@interface ProfileController : NSObject

// Contains information about the Profile state.
@property(nonatomic, readonly) ProfileState* state;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_CONTROLLER_H_
