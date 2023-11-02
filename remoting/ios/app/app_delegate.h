// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_APP_DELEGATE_H_
#define REMOTING_IOS_APP_APP_DELEGATE_H_

#import <UIKit/UIKit.h>

// Default created delegate class for the entire application.
@interface AppDelegate : UIResponder<UIApplicationDelegate>

@property(strong, nonatomic) UIWindow* window;
@property(class, strong, nonatomic, readonly) AppDelegate* instance;

// This will push the Help Center view controller onto the provided nav
// controller.
- (void)navigateToHelpCenter:(UINavigationController*)navigationController;

// Presents contents modally onto the topmost view controller.
- (void)presentHelpCenter;

// TODO(yuweih): Replace calls to this method with methods from HelpAndFeedback.
// This will present the Send Feedback view controller onto the topmost view
// controller.
// context: a unique identifier for the user's place within the app which can be
// used to categorize the feedback report and segment usage metrics.
- (void)presentFeedbackFlowWithContext:(NSString*)context;

// Pop up an Email compose view filled with the instructions to setup the host.
- (void)emailSetupInstructions;

@end

#endif  // REMOTING_IOS_APP_APP_DELEGATE_H_

