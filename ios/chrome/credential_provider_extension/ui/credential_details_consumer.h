// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_CONSUMER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_CONSUMER_H_

#include "ios/chrome/common/credential_provider/credential.h"

@class UIButton;

@protocol CredentialDetailsConsumerDelegate <NSObject>

// Called when the user taps the cancel button in the navigation bar.
- (void)navigationCancelButtonWasPressed:(UIButton*)button;

// Called when the user selects a credential.
- (void)userSelectedCredential:(id<Credential>)credential;

// Called when the user requests a clear view of the password. The delegate
// should complete with the clear password or nil in case of failure or
// deny by user.
- (void)unlockPasswordForCredential:(id<Credential>)credential
                  completionHandler:(void (^)(NSString*))completionHandler;

@end

@protocol CredentialDetailsConsumer <NSObject>

// The delegate for the actions in the consumer.
@property(nonatomic, weak) id<CredentialDetailsConsumerDelegate> delegate;

// Tells the consumer to show the `credential` details.
- (void)presentCredential:(id<Credential>)credential;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_CONSUMER_H_
