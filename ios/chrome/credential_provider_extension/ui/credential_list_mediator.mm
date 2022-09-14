// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "ios/chrome/common/credential_provider/credential_store.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_ui_handler.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "ios/chrome/credential_provider_extension/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// The extension context in which the credential list was started.
@property(nonatomic, weak) ASCredentialProviderExtensionContext* context;

// List of suggested credentials.
@property(nonatomic, copy) NSArray<id<Credential>>* suggestedCredentials;

// List of all credentials.
@property(nonatomic, copy) NSArray<id<Credential>>* allCredentials;

@end

@implementation CredentialListMediator

- (instancetype)initWithConsumer:(id<CredentialListConsumer>)consumer
                       UIHandler:(id<CredentialListUIHandler>)UIHandler
                 credentialStore:(id<CredentialStore>)credentialStore
                         context:(ASCredentialProviderExtensionContext*)context
              serviceIdentifiers:
                  (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  self = [super init];
  if (self) {
    _serviceIdentifiers = serviceIdentifiers ?: @[];
    _UIHandler = UIHandler;
    _consumer = consumer;
    _consumer.delegate = self;
    _credentialStore = credentialStore;
    _context = context;
  }
  return self;
}

- (void)fetchCredentials {
  [self.consumer
      setTopPrompt:PromptForServiceIdentifiers(self.serviceIdentifiers)];

  dispatch_queue_t priorityQueue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul);
  dispatch_async(priorityQueue, ^{
    self.allCredentials = [self.credentialStore.credentials
        sortedArrayUsingComparator:^NSComparisonResult(id<Credential> obj1,
                                                       id<Credential> obj2) {
          return [obj1.serviceName compare:obj2.serviceName];
        }];

    NSMutableArray* suggestions = [[NSMutableArray alloc] init];
    for (id<Credential> credential in self.allCredentials) {
      for (ASCredentialServiceIdentifier* identifier in self
               .serviceIdentifiers) {
        if (credential.serviceName &&
            [identifier.identifier
                localizedStandardContainsString:credential.serviceName]) {
          [suggestions addObject:credential];
          break;
        }
        if (credential.serviceIdentifier &&
            [identifier.identifier
                localizedStandardContainsString:credential.serviceIdentifier]) {
          [suggestions addObject:credential];
          break;
        }
      }
    }
    self.suggestedCredentials = suggestions;

    dispatch_async(dispatch_get_main_queue(), ^{
      // TODO(crbug.com/1297158): Remove the serviceIdentifier check once the
      // new password screen properly supports user url entry.
      BOOL canCreatePassword =
          IsPasswordCreationUserEnabled() && self.serviceIdentifiers.count > 0;
      if (!canCreatePassword && !self.allCredentials.count) {
        [self.UIHandler showEmptyCredentials];
        return;
      }
      [self.consumer presentSuggestedPasswords:self.suggestedCredentials
                                  allPasswords:self.allCredentials
                                 showSearchBar:self.allCredentials.count > 0
                         showNewPasswordOption:canCreatePassword];
    });
  });
}

#pragma mark - CredentialListHandler

- (void)navigationCancelButtonWasPressed:(UIButton*)button {
  NSError* error =
      [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                 code:ASExtensionErrorCodeUserCanceled
                             userInfo:nil];
  [self.context cancelRequestWithError:error];
}

- (void)userSelectedCredential:(id<Credential>)credential {
  [self.UIHandler userSelectedCredential:credential];
}

- (void)updateResultsWithFilter:(NSString*)filter {
  // TODO(crbug.com/1297158): Remove the serviceIdentifier check once the
  // new password screen properly supports user url entry.
  BOOL showNewPasswordOption = !filter.length &&
                               IsPasswordCreationUserEnabled() &&
                               self.serviceIdentifiers.count > 0;
  if (!filter.length) {
    [self.consumer presentSuggestedPasswords:self.suggestedCredentials
                                allPasswords:self.allCredentials
                               showSearchBar:YES
                       showNewPasswordOption:showNewPasswordOption];
    return;
  }

  NSMutableArray<id<Credential>>* suggested = [[NSMutableArray alloc] init];
  for (id<Credential> credential in self.suggestedCredentials) {
    if ([credential.serviceName localizedStandardContainsString:filter] ||
        [credential.user localizedStandardContainsString:filter]) {
      [suggested addObject:credential];
    }
  }

  NSMutableArray<id<Credential>>* all = [[NSMutableArray alloc] init];
  for (id<Credential> credential in self.allCredentials) {
    if ([credential.serviceName localizedStandardContainsString:filter] ||
        [credential.user localizedStandardContainsString:filter]) {
      [all addObject:credential];
    }
  }
  [self.consumer presentSuggestedPasswords:suggested
                              allPasswords:all
                             showSearchBar:YES
                     showNewPasswordOption:showNewPasswordOption];
}

- (void)showDetailsForCredential:(id<Credential>)credential {
  [self.UIHandler showDetailsForCredential:credential];
}

- (void)newPasswordWasSelected {
  [self.UIHandler showCreateNewPasswordUI];
}

@end
