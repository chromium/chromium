// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_ROOT_COORDINATOR_ROOT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_ROOT_COORDINATOR_ROOT_COORDINATOR_H_

#import <UIKit/UIKit.h>

/// A class that represents the root coordinator for the app.
/// It has a similar interface to ChromeCoordinator, but does not have any
/// browser, since the app may have more than one browser at a time.
@interface RootCoordinator : NSObject
// The base view controller that is created by this coordinator.
// Subclasses are required to initialize this property.
@property(weak, nonatomic) UIViewController* baseViewController;

// Starts the coordinator. The default implementation is no-op.
- (void)start;
// Stops the coordinator. The default implementation is no-op.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_ROOT_COORDINATOR_ROOT_COORDINATOR_H_
