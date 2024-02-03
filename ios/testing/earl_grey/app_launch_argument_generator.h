// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_APP_LAUNCH_ARGUMENT_GENERATOR_H_
#define IOS_TESTING_EARL_GREY_APP_LAUNCH_ARGUMENT_GENERATOR_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/app_launch_configuration.h"

// Returns an array of arguments to launch the app with from the given
// `configuration`.
NSArray<NSString*>* ArgumentsFromConfiguration(
    AppLaunchConfiguration configuration);

#endif  // IOS_TESTING_EARL_GREY_APP_LAUNCH_ARGUMENT_GENERATOR_H_
