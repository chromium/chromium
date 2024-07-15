// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_MEDIATOR_TESTING_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_MEDIATOR_TESTING_H_

#import "ios/chrome/credential_provider_extension/ui/credential_list_mediator.h"

// Unit test interface for CredentialListMediator.
@interface CredentialListMediator (Testing)

// Sets the list of suggested credentials.
- (void)setSuggestedCredentials:(NSArray<id<Credential>>*)suggestedCredentials;

// Sets the list of all credentials.
- (void)setAllCredentials:(NSArray<id<Credential>>*)allCredentials;

// Returns all credentials from the credential store, filtered by request type
// and sorted by service name.
- (NSArray<id<Credential>>*)fetchAllCredentials;

// Returns the list of allowed passkey credentials for the relying party.
- (NSArray<id<Credential>>*)filterPasskeyCredentials;

// Returns the list of allowed password credentials for the service identifier.
- (NSArray<id<Credential>>*)filterPasswordCredentials;

// Tells the consumer to show the passed in suggested and all credentials.
- (void)presentCredentials;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_MEDIATOR_TESTING_H_
