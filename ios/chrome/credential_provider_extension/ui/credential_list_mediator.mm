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

  __weak __typeof(self) weakSelf = self;
  dispatch_queue_t priorityQueue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul);
  dispatch_async(priorityQueue, ^{
    [weakSelf fetchAndPresentRelevantCredentials];
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

// Fetches and presents credentials that are relevant to the service the user is
// trying to log into.
- (void)fetchAndPresentRelevantCredentials {
  self.allCredentials = [self fetchAllCredentials];
  self.suggestedCredentials = [self filterCredentials];

  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf presentCredentials];
  });
}

// Returns all credentials from the credential store, filtered by request type
// and sorted by service name.
- (NSArray<id<Credential>>*)fetchAllCredentials {
  NSString* relyingPartyIdentifier = [self.UIHandler relyingPartyIdentifier];

  // Figure out which type(s) of credentials to keep.
  BOOL includePasswords = NO;
  BOOL includePasskeys = NO;
  if (relyingPartyIdentifier) {
    // When showing passkeys, only include passwords if there's at least one
    // that matches the service identifiers.
    includePasswords = [self hasPasswordThatMatchesServiceIdentifiers];
    includePasskeys = YES;
  } else {
    includePasswords = YES;
  }

  NSArray<id<Credential>>* credentials = [self.credentialStore.credentials
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                   id<Credential> credential,
                                                   NSDictionary* bindings) {
        BOOL isPassword = !credential.isPasskey;
        BOOL isValidPasskey =
            credential.isPasskey && !credential.hidden &&
            [credential.rpId isEqualToString:relyingPartyIdentifier];

        return (includePasswords && isPassword) ||
               (includePasskeys && isValidPasskey);
      }]];

  // Only sort if there's no relying party identifier. Otherwise, it means that
  // the `credentials` list only contains passkeys, and hence there's no need to
  // sort as they all have the same `rpId`.
  if (!relyingPartyIdentifier) {
    credentials = [credentials sortedArrayUsingComparator:^NSComparisonResult(
                                   id<Credential> obj1, id<Credential> obj2) {
      NSString* firstIdentifier = obj1.isPasskey ? obj1.rpId : obj1.serviceName;
      NSString* secondIdentifier =
          obj2.isPasskey ? obj2.rpId : obj2.serviceName;
      return [firstIdentifier compare:secondIdentifier];
    }];
  }

  return credentials;
}

// Returns the list of allowed credentials that are related to the relying
// party/service identifiers.
- (NSArray<id<Credential>>*)filterCredentials {
  NSMutableArray* filteredCredentials = [[NSMutableArray alloc] init];
  NSArray<NSData*>* allowedCredentials = [self.UIHandler allowedCredentials];
  // If the `allowedCredentials` array is empty, then the relying party accepts
  // any passkey credential.
  BOOL isAnyPasskeyAllowed = allowedCredentials.count == 0;

  for (id<Credential> credential in self.allCredentials) {
    if (credential.isPasskey) {
      if (isAnyPasskeyAllowed ||
          [allowedCredentials containsObject:credential.credentialId]) {
        [filteredCredentials addObject:credential];
      }
    } else if (!credential.isPasskey &&
               [self passwordCredential:credential
                   matchesServiceIdentifiers:self.serviceIdentifiers]) {
      [filteredCredentials addObject:credential];
    }
  }

  return filteredCredentials;
}

// Returns `YES` if the provided `credential` matches at least one of the
// `serviceIdentifiers`.
- (BOOL)passwordCredential:(id<Credential>)credential
    matchesServiceIdentifiers:
        (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  for (ASCredentialServiceIdentifier* identifier in serviceIdentifiers) {
    BOOL serviceNameMatches =
        credential.serviceName &&
        [identifier.identifier
            localizedStandardContainsString:credential.serviceName];
    BOOL serviceIdentifierMatches =
        credential.serviceIdentifier &&
        [identifier.identifier
            localizedStandardContainsString:credential.serviceIdentifier];
    if (serviceNameMatches || serviceIdentifierMatches) {
      return YES;
    }
  }
  return NO;
}

// Returns `YES` if at least one of the saved password credentials matches the
// `serviceIdentifiers`.
- (BOOL)hasPasswordThatMatchesServiceIdentifiers {
  __weak __typeof(self) weakSelf = self;
  NSUInteger indexOfMatchingPassword = [self.credentialStore.credentials
      indexOfObjectPassingTest:^BOOL(id<Credential> credential, NSUInteger,
                                     BOOL*) {
        return !credential.isPasskey &&
               [weakSelf passwordCredential:credential
                   matchesServiceIdentifiers:weakSelf.serviceIdentifiers];
      }];
  return indexOfMatchingPassword != NSNotFound;
}

// Tells the consumer to show the passed in suggested and all credentials.
- (void)presentCredentials {
  // TODO(crbug.com/40215043): Remove the serviceIdentifier check once the
  // new password screen properly supports user url entry.
  BOOL canCreatePassword = ![self.UIHandler relyingPartyIdentifier] &&
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
