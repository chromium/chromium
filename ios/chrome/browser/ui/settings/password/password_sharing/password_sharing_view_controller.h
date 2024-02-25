// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol PasswordSharingViewControllerPresentationDelegate;

// Displays empty password sharing bottom sheet with a spinner, visible until
// password sharing recipients are fetched.
@interface PasswordSharingViewController : UIViewController

// Delegate for handling dismissal of the view.
@property(nonatomic, weak) id<PasswordSharingViewControllerPresentationDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_VIEW_CONTROLLER_H_
