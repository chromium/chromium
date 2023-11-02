// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_FIRST_LAUNCH_VIEW_PRESENTER_H_
#define REMOTING_IOS_APP_FIRST_LAUNCH_VIEW_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "remoting/ios/app/first_launch_view_controller.h"

// A class to present the first launch view when the user is signed out. The
// class will immediately start tracking the user state once it's initialized.
// Other components should NEVER push/pop/replace the top view controller of
// |navController|.
@interface FirstLaunchViewPresenter : NSObject

- (instancetype)initWithNavController:(UINavigationController*)navController
               viewControllerDelegate:
                   (id<FirstLaunchViewControllerDelegate>)delegate;

// Forcibly present the first launch view. Do nothing if the view is already
// presented.
- (void)presentView;

@end

#endif  // REMOTING_IOS_APP_FIRST_LAUNCH_VIEW_PRESENTER_H_
