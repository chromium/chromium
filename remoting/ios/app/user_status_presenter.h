// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_USER_STATUS_PRESENTER_H_
#define REMOTING_IOS_APP_USER_STATUS_PRESENTER_H_

#import <UIKit/UIKit.h>

// A simple utility that presents toast and show the user info when the user
// switches account or just logs in.
@interface UserStatusPresenter : NSObject

- (void)start;

- (void)stop;

@property(nonatomic, readonly, class) UserStatusPresenter* instance;

@end

#endif  // REMOTING_IOS_APP_USER_STATUS_PRESENTER_H_
