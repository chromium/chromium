// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_COMMON_COORDINATOR_H_
#define IOS_SHOWCASE_COMMON_COORDINATOR_H_

#import <UIKit/UIKit.h>

// This protocol is the common interface to the simple non-production
// coordinators that will initialize, set up and present production view
// controllers, usually with completely mocked data.
@protocol Coordinator<NSObject>

// The base view controller used to present the view controller that this
// coordinator drives.
@property(nonatomic, weak) UIViewController* baseViewController;

// Typically, this initializes a view controller, sets it up and presents it.
- (void)start;

@optional

// Typically, this stops a view controller.
- (void)stop;

@end

#endif  // IOS_SHOWCASE_COMMON_COORDINATOR_H_
