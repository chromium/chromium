// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_FIRST_LAUNCH_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_FIRST_LAUNCH_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol FirstLaunchViewControllerDelegate<NSObject>

- (void)presentSignInFlow;

@end

// This shows a view to ask the user to sign in.
@interface FirstLaunchViewController : UIViewController

@property(nonatomic, weak) id<FirstLaunchViewControllerDelegate> delegate;

@end

#endif  // REMOTING_IOS_APP_FIRST_LAUNCH_VIEW_CONTROLLER_H_
