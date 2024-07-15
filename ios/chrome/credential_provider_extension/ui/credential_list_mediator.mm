// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "ios/chrome/common/credential_provider/credential_store.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_ui_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "ios/chrome/credential_provider_extension/ui/ui_util.h"

@interface CredentialListMediator () <CredentialListHandler>

// The UI Handler of the feature.
@property(nonatomic, weak) id<CredentialListUIHandler> UIHandler;

// The consumer for this mediator.
@property(nonatomic, weak) id<CredentialListConsumer> consumer;

// Interface for the persistent credential store.
@property(nonatomic, weak) id<CredentialStore> credentialStore;

// The service identifiers to be prioritized.
@property(nonatomic, strong)
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers;

// List of suggested credentials.
@property(nonatomic, copy) NSArray<id<Credential>>* suggestedCredentials;

// List of all credentials.
@property(nonatomic, copy) NSArray<id<Credential>>* allCredentials;

// The response handler for any credential actions.
@property(nonatomic, weak) id<CredentialResponseHandler>
    credentialResponseHandler;

@end

@implementation CredentialListMediator

- (instancetype)initWithConsumer:(id<CredentialListConsumer>)consumer
                       UIHandler:(id<CredentialListUIHandler>)UIHandler
                 credentialStore:(id<CredentialStore>)credentialStore
              serviceIdentifiers:
                  (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
       credentialResponseHandler:
           (id<CredentialResponseHandler>)credentialResponseHandler {
  self = [super init];
  if (self) {
    _serviceIdentifiers = serviceIdentifiers ?: @[];
    _UIHandler = UIHandler;
    _consumer = consumer;
    _consumer.delegate = self;
    _credentialStore = credentialStore;
    _credentialResponseHandler = credentialResponseHandler;
  }
  return self;
}

- (void)fetchCredentials {
  [self.consumer
      setTopPrompt:PromptForServiceIdentifiers(self.serviceIdentifiers)];

  dispatch_queue_t priorityQueue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul);
  dispatch_async(priorityQueue, ^{
    self.allCredentials = [self fetchAllCredentials];

    self.suggestedCredentials = [self.UIHandler isRequestingPasskey]
                                    ? [self filterPasskeyCredentials]
                                    : [self filterPasswordCredentials];

    dispatch_async(dispatch_get_main_queue(), ^{
      [self presentCredentials];
    });
  });
}

#pragma mark - CredentialListHandler

- (void)navigationCancelButtonWasPressed:(UIButton*)button {
  [self.credentialResponseHandler
      userCancelledRequestWithErrorCode:ASExtensionErrorCodeUserCanceled];
}

- (void)userSelectedCredential:(id<Credential>)credential {
  [self.UIHandler userSelectedCredential:credential];
}

- (void)updateResultsWithFilter:(NSString*)filter {
  // TODO(crbug.com/40215043): Remove the serviceIdentifier check once the
  // new password screen properly supports user url entry.
  BOOL showNewPasswordOption = !filter.length &&
                               IsPasswordCreationUserEnabled() &&
                               self.serviceIdentifiers.count > 0;
  if (!filter.length) {
    [self.consumer presentSuggestedCredentials:self.suggestedCredentials
                                allCredentials:self.allCredentials
                                 showSearchBar:YES
                         showNewPasswordOption:showNewPasswordOption];
    return;
  }

  NSMutableArray<id<Credential>>* suggested = [[NSMutableArray alloc] init];
  for (id<Credential> credential in self.suggestedCredentials) {
    if ([credential.serviceName localizedStandardContainsString:filter] ||
        [credential.username localizedStandardContainsString:filter]) {
      [suggested addObject:credential];
    }
  }

  NSMutableArray<id<Credential>>* all = [[NSMutableArray alloc] init];
  for (id<Credential> credential in self.allCredentials) {
    if ([credential.serviceName localizedStandardContainsString:filter] ||
        [credential.username localizedStandardContainsString:filter]) {
      [all addObject:credential];
    }
  }
  [self.consumer presentSuggestedCredentials:suggested
                              allCredentials:all
                               showSearchBar:YES
                       showNewPasswordOption:showNewPasswordOption];
}

- (void)showDetailsForCredential:(id<Credential>)credential {
  [self.UIHandler showDetailsForCredential:credential];
}

- (void)newPasswordWasSelected {
  [self.UIHandler showCreateNewPasswordUI];
}

#pragma mark - Private

// Returns all credentials from the credential store, filtered by request type
// and sorted by service name.
- (NSArray<id<Credential>>*)fetchAllCredentials {
  BOOL isRequestingPasskey = [self.UIHandler isRequestingPasskey];
  // Only use passwords or passkeys, depending on what's requested.
  NSArray<id<Credential>>* credentials = [self.credentialStore.credentials
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                   id<Credential> credential,
                                                   NSDictionary* bindings) {
        return credential.isPasskey == isRequestingPasskey;
      }]];

  credentials = [credentials sortedArrayUsingComparator:^NSComparisonResult(
                                 id<Credential> obj1, id<Credential> obj2) {
    return isRequestingPasskey ? [obj1.rpId compare:obj2.rpId]
                               : [obj1.serviceName compare:obj2.serviceName];
  }];
  return credentials;
}

// Returns the list of allowed passkey credentials for the relying party.
- (NSArray<id<Credential>>*)filterPasskeyCredentials {
  // If the allowedCredentials array is empty, then the relying party accepts
  // any passkey credential.
  NSArray<NSData*>* allowedCredentials = [self.UIHandler allowedCredentials];
  if (allowedCredentials.count == 0) {
    return self.allCredentials;
  }

  NSMutableArray* filteredCredentials = [[NSMutableArray alloc] init];
  for (id<Credential> credential in self.allCredentials) {
    if ([allowedCredentials containsObject:credential.credentialId]) {
      [filteredCredentials addObject:credential];
    }
  }
  return filteredCredentials;
}

// Returns the list of allowed password credentials for the service identifier.
- (NSArray<id<Credential>>*)filterPasswordCredentials {
  NSMutableArray* filteredCredentials = [[NSMutableArray alloc] init];
  for (id<Credential> credential in self.allCredentials) {
    for (ASCredentialServiceIdentifier* identifier in self.serviceIdentifiers) {
      if (credential.serviceName &&
          [identifier.identifier
              localizedStandardContainsString:credential.serviceName]) {
        [filteredCredentials addObject:credential];
        break;
      }
      if (credential.serviceIdentifier &&
          [identifier.identifier
              localizedStandardContainsString:credential.serviceIdentifier]) {
        [filteredCredentials addObject:credential];
        break;
      }
    }
  }
  return filteredCredentials;
}

// Tells the consumer to show the passed in suggested and all credentials.
- (void)presentCredentials {
  // TODO(crbug.com/40215043): Remove the serviceIdentifier check once the
  // new password screen properly supports user url entry.
  BOOL canCreatePassword = ![self.UIHandler isRequestingPasskey] &&
                           IsPasswordCreationUserEnabled() &&
                           self.serviceIdentifiers.count > 0;
  if (!canCreatePassword && !self.allCredentials.count) {
    [self.UIHandler showEmptyCredentials];
    return;
  }
  [self.consumer presentSuggestedCredentials:self.suggestedCredentials
                              allCredentials:self.allCredentials
                               showSearchBar:self.allCredentials.count > 0
                       showNewPasswordOption:canCreatePassword];
}

@end
