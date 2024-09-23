// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_COORDINATOR_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_COORDINATOR_H_

#import <Foundation/Foundation.h>

@class ASCredentialServiceIdentifier;
@class ASPasskeyCredentialRequestParameters;
@protocol CredentialResponseHandler;
@protocol CredentialStore;
@class ReauthenticationHandler;
@class UIViewController;

// This feature presents a list of credentials for the user to choose.
@interface CredentialListCoordinator : NSObject

// Default initializer. When the coordinator is started it will present on
// `baseViewController`. `serviceIdentifiers` will be used to prioritize data,
// can be nil.
- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
               credentialStore:(id<CredentialStore>)credentialStore
            serviceIdentifiers:
                (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
       reauthenticationHandler:(ReauthenticationHandler*)reauthenticationHandler
     credentialResponseHandler:
         (id<CredentialResponseHandler>)credentialResponseHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the credential list.
- (void)start;

// Stops the credential list.
- (void)stop;

// Set the request parameters for passkeys.
- (void)setRequestParameters:
    (ASPasskeyCredentialRequestParameters*)requestParameters
    API_AVAILABLE(ios(17.0));

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_COORDINATOR_H_
