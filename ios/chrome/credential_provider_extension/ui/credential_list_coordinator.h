// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_COORDINATOR_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_COORDINATOR_H_

#import <Foundation/Foundation.h>

@class ASCredentialServiceIdentifier;
@class ASCredentialProviderExtensionContext;
@protocol CredentialStore;
@class ReauthenticationHandler;
@class UIViewController;

// This feature presents a list of credentials for the user to choose.
@interface CredentialListCoordinator : NSObject

// Default initializer. When the coordinator is started it will present on
// |baseViewController|. |serviceIdentifiers| will be used to prioritize data,
// can be nil.
- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
               credentialStore:(id<CredentialStore>)credentialStore
                       context:(ASCredentialProviderExtensionContext*)context
            serviceIdentifiers:
                (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
       reauthenticationHandler:(ReauthenticationHandler*)reauthenticationHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the credential list.
- (void)start;

// Stops the credential list.
- (void)stop;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_COORDINATOR_H_
