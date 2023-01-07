// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HOST_SETUP_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_HOST_SETUP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// This controller shows instruction for setting up the host a host when the
// user has no host in the host list.
@interface HostSetupViewController : UITableViewController

@property(weak, nonatomic) id<UIScrollViewDelegate> scrollViewDelegate;

@end

#endif  // REMOTING_IOS_APP_HOST_SETUP_VIEW_CONTROLLER_H_
