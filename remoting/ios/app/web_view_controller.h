// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_WEB_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_WEB_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// A simple VC for showing a web view for the given url. It will show a close
// navigation button when it is the first VC in the navigation stack.
@interface WebViewController : UIViewController

- (instancetype)initWithUrl:(NSString*)url title:(NSString*)title;

@end

#endif  // REMOTING_IOS_APP_WEB_VIEW_CONTROLLER_H_
