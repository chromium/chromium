// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_APP_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_APP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// DEPRECATED.
// TODO(yuweih): Remove this file once the down stream implementation is
// removed.

@protocol AppController<NSObject>

// For adding new methods, please mark them as optional until they are
// implemented in both Chromium and ios_internal. For deletion, just reverse the
// procedure. This is to prevent build break in internal buildbot.

- (void)showMenuAnimated:(BOOL)animated;
- (void)hideMenuAnimated:(BOOL)animated;
- (void)presentSignInFlow;

@end

// The root view controller of the app. It acts as a container controller for
// |mainViewController|, which will be shown as the primary content of the view.
// Implementation can add drawer or modal for things like the side menu or help
// and feedback.
@interface AppViewController : UIViewController<AppController>

- (instancetype)initWithMainViewController:
    (UIViewController*)mainViewController;

@end

#endif  // REMOTING_IOS_APP_APP_VIEW_CONTROLLER_H_
