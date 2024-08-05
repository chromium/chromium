// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/credential_provider_extension/ui/new_password_ui_handler.h"

@class ArchivableCredential;
@protocol Credential;
@class NewPasswordViewController;

@protocol NewPasswordViewControllerDelegate <NSObject>

// Called when the user taps the cancel button in the navigation bar.
- (void)navigationCancelButtonWasPressedInNewPasswordViewController:
    (NewPasswordViewController*)viewController;

@end

@protocol NewCredentialHandler

// User tapped on "Generate Strong Password" button.
- (void)userDidRequestGeneratedPassword;

// Asks the handler to save a credential with the given `username`, `password`
// and `note`. If `shouldReplace` is true, then the user has already been warned
// that they may be replacing an existing credential. Otherwise, the handler
// should not replace an existing credential.
- (void)saveCredentialWithUsername:(NSString*)username
                          password:(NSString*)password
                              note:(NSString*)note
                              gaia:(NSString*)gaia
                     shouldReplace:(BOOL)shouldReplace;

@end

// View Controller where a user can create a new credential and use a suggested
// password.
@interface NewPasswordViewController
    : UITableViewController <NewPasswordUIHandler>

@property(nonatomic, weak) id<NewPasswordViewControllerDelegate> delegate;

@property(nonatomic, weak) id<NewCredentialHandler> credentialHandler;

// The host for the password being generated.
@property(nonatomic, strong) NSString* currentHost;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_VIEW_CONTROLLER_H_
