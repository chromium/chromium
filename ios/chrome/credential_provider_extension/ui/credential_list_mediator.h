// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_MEDIATOR_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class ASCredentialServiceIdentifier;
@protocol CredentialListConsumer;
@protocol CredentialListUIHandler;
@protocol CredentialResponseHandler;
@protocol CredentialStore;

// This mediator fetches and organizes the credentials for its consumer.
@interface CredentialListMediator : NSObject

// `serviceIdentifiers` will be used to prioritize data, can be nil.
- (instancetype)initWithConsumer:(id<CredentialListConsumer>)consumer
                       UIHandler:(id<CredentialListUIHandler>)UIHandler
                 credentialStore:(id<CredentialStore>)credentialStore
              serviceIdentifiers:
                  (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
       credentialResponseHandler:
           (id<CredentialResponseHandler>)credentialResponseHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Gets the available credentials and posts them to the consumer.
- (void)fetchCredentials;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_MEDIATOR_H_
