// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_CONSUMER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_CONSUMER_H_

#include "ios/chrome/common/credential_provider/credential.h"

@class UIButton;

@protocol CredentialListHandler <NSObject>

// Called when the user taps the cancel button in the navigation bar.
- (void)navigationCancelButtonWasPressed:(UIButton*)button;

// Called when the user selects a credential.
- (void)userSelectedCredential:(id<Credential>)credential;

// Called when the user is filtering results through search.
- (void)updateResultsWithFilter:(NSString*)filter;

// Called when user wants to see details for the given credential.
- (void)showDetailsForCredential:(id<Credential>)credential;

// Called when user taps the option to create a new password
- (void)newPasswordWasSelected;

@end

@protocol CredentialListConsumer <NSObject>

// The delegate for the actions in the consumer.
@property(nonatomic, weak) id<CredentialListHandler> delegate;

// Tells the consumer to show the passed in suggested and all credentials.
- (void)presentSuggestedCredentials:(NSArray<id<Credential>>*)suggested
                     allCredentials:(NSArray<id<Credential>>*)all
                      showSearchBar:(BOOL)showSearchBar
              showNewPasswordOption:(BOOL)showNewPasswordOption;

// Sets the prompt to show for the view.
- (void)setTopPrompt:(NSString*)prompt;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_CONSUMER_H_
