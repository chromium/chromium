// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class NewPasswordViewController;

@protocol NewPasswordViewControllerDelegate <NSObject>

// Called when the user taps the cancel button in the navigation bar.
- (void)navigationCancelButtonWasPressedInNewPasswordViewController:
    (NewPasswordViewController*)viewController;

@end

// View Controller where a user can create a new credential and use a suggested
// password.
@interface NewPasswordViewController : UITableViewController

@property(nonatomic, weak) id<NewPasswordViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_
