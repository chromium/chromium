// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_COMMON_NAVIGATION_COORDINATOR_H_
#define IOS_SHOWCASE_COMMON_NAVIGATION_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/showcase/common/coordinator.h"

// This protocol is a specialization of the Coordinator protocol when the driven
// view controller is pushed on a navigation controller.
@protocol NavigationCoordinator<Coordinator>

// Redefined to be a UINavigationController.
@property(nonatomic, weak) UINavigationController* baseViewController;

@end

#endif  // IOS_SHOWCASE_COMMON_NAVIGATION_COORDINATOR_H_
