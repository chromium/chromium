// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_DELEGATE_H_

@class CredentialDetails;
@class AddPasswordViewController;

@protocol AddPasswordViewControllerDelegate

// Called when user finished adding a new password credential.
- (void)addPasswordViewController:(AddPasswordViewController*)viewController
            didAddPasswordDetails:(NSString*)username
                         password:(NSString*)password
                             note:(NSString*)note;

// Called on every keystroke to check whether duplicates exist before adding a
// new credential.
- (void)checkForDuplicates:(NSString*)username;

// Called when an existing credential is to be displayed in the add credential
// flow.
- (void)showExistingCredential:(NSString*)username;

// Called when the user cancels the add password view.
- (void)didCancelAddPasswordDetails;

// Called every time the text in the website field is updated.
- (void)setWebsiteURL:(NSString*)website;

// Returns whether the website URL has http(s) scheme and is valid or not.
- (BOOL)isURLValid;

// Called to check if the url is missing the top-level domain.
- (BOOL)isTLDMissing;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_VIEW_CONTROLLER_DELEGATE_H_
