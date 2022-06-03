// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_APP_INTERFACE_H_
#define IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// AppLaunchManagerAppInterface contains helpers for AppLaunchManager
// that are compiled into the app binary and can be called from either app or
// test code.
@interface AppLaunchManagerAppInterface : NSObject

// Returns current host app PID.
+ (int)processIdentifier;

@end

#endif  // IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_APP_INTERFACE_H_
