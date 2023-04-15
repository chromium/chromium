// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_CONNECTION_INFORMATION_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_CONNECTION_INFORMATION_H_

#import <Foundation/Foundation.h>

@class AppStartupParameters;

// Contains information about the initialization of scenes.
@protocol ConnectionInformation <NSObject>

// Parameters received when the scene is connected. These parameters are stored
// to be executed when the scene reach the required state.
@property(nonatomic, strong) AppStartupParameters* startupParameters;

// Flag that is set when the `startupParameters` start being handled.
// Checking this flag prevents reentrant startup parameter handling.
@property(nonatomic, assign) BOOL startupParametersAreBeingHandled;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_CONNECTION_INFORMATION_H_
