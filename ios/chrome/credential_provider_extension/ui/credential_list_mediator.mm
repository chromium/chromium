// Copyright 2020 The Chromium Authors. All rights reserved.
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
  if (IsPasswordCreationEnabled()) {
    [self.consumer
        setTopPrompt:PromptForServiceIdentifiers(self.serviceIdentifiers)];
  } else {
    NSString* identifier = self.serviceIdentifiers.firstObject.identifier;
    NSURL* promptURL = identifier ? [NSURL URLWithString:identifier] : nil;
    [self.consumer setTopPrompt:promptURL.host];
  }

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
        if ([identifier.identifier containsString:credential.serviceName]) {
          [suggestions addObject:credential];
          break;
        }
      }
    }
    self.suggestedCredentials = suggestions;

    dispatch_async(dispatch_get_main_queue(), ^{
      if (!IsPasswordCreationEnabled() && !self.allCredentials.count) {
        [self.UIHandler showEmptyCredentials];
        return;
      }
      [self.consumer presentSuggestedPasswords:self.suggestedCredentials
                                  allPasswords:self.allCredentials
                         showNewPasswordOption:IsPasswordCreationEnabled()];
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
  NSMutableArray<id<Credential>>* suggested = [[NSMutableArray alloc] init];
  if (self.suggestedCredentials.count > 0) {
    for (id<Credential> credential in self.suggestedCredentials) {
      if ([filter length] == 0 ||
          [credential.serviceName localizedStandardContainsString:filter] ||
          [credential.user localizedStandardContainsString:filter]) {
        [suggested addObject:credential];
      }
    }
  }
  NSMutableArray<id<Credential>>* all = [[NSMutableArray alloc] init];
  if (self.allCredentials.count > 0) {
    for (id<Credential> credential in self.allCredentials) {
      if ([filter length] == 0 ||
          [credential.serviceName localizedStandardContainsString:filter] ||
          [credential.user localizedStandardContainsString:filter]) {
        [all addObject:credential];
      }
    }
  }
  BOOL showNewPasswordOption = !filter.length && IsPasswordCreationEnabled();
  [self.consumer presentSuggestedPasswords:suggested
                              allPasswords:all
                     showNewPasswordOption:showNewPasswordOption];
}

- (void)showDetailsForCredential:(id<Credential>)credential {
  [self.UIHandler showDetailsForCredential:credential];
}

- (void)newPasswordWasSelected {
  [self.UIHandler showCreateNewPasswordUI];
}

@end
