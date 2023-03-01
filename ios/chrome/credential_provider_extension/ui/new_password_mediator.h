// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_MEDIATOR_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"

@class ASCredentialServiceIdentifier;
@protocol CredentialResponseHandler;
@protocol CredentialStore;
@protocol NewPasswordUIHandler;

// This mediator fetches requirements and saves new credentials for its
// consumer.
@interface NewPasswordMediator : NSObject <NewCredentialHandler>

// Initializes a new object, using `userDefaults` as the user defaults location
// to store new credentials to and `serviceIdentifier` as the current service to
// store new credentials for.
- (instancetype)initWithUserDefaults:(NSUserDefaults*)userDefaults
                   serviceIdentifier:
                       (ASCredentialServiceIdentifier*)serviceIdentifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Existing credential store to check to see if a new credential already
// exists.
@property(nonatomic, weak) id<CredentialStore> existingCredentials;

// UI handler to allow this mediator to ask the UI for any necessary updates.
@property(nonatomic, weak) id<NewPasswordUIHandler> uiHandler;

// The handler to use when a credential is selected or cancelled.
@property(nonatomic, weak) id<CredentialResponseHandler>
    credentialResponseHandler;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_MEDIATOR_H_
