// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_

#import <Foundation/Foundation.h>

class ChromeBrowserState;

// Represents the state for a single Profile and responds to the state
// changes and system events.
@interface ProfileState : NSObject

// The non-incognito ChromeBrowserState used for this Profile.
@property(nonatomic, assign) ChromeBrowserState* browserState;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
