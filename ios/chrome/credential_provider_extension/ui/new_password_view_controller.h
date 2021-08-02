// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ArchivableCredential;
@protocol Credential;
@class NewPasswordViewController;

@protocol NewPasswordViewControllerDelegate <NSObject>

// Called when the user taps the cancel button in the navigation bar.
- (void)navigationCancelButtonWasPressedInNewPasswordViewController:
    (NewPasswordViewController*)viewController;

// Called when the user selects a given credential
- (void)userSelectedCredential:(id<Credential>)credential;

@end

@protocol NewCredentialHandler

// Called when the user wants to create a new credential.
- (ArchivableCredential*)createNewCredentialWithUsername:(NSString*)username
                                                password:(NSString*)password;

// Saves the given credential to disk and calls |completion| once the operation
// is finished.
- (void)saveNewCredential:(ArchivableCredential*)credential
               completion:(void (^)(NSError* error))completion;

@end

// View Controller where a user can create a new credential and use a suggested
// password.
@interface NewPasswordViewController : UITableViewController

@property(nonatomic, weak) id<NewPasswordViewControllerDelegate> delegate;

@property(nonatomic, weak) id<NewCredentialHandler> credentialHandler;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_
