// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/proto/password_requirements.pb.h"
#import "components/password_manager/core/browser/generation/password_generator.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_util.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_store.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/password_spec_fetcher_buildflags.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_ui_handler.h"
#import "ios/chrome/credential_provider_extension/ui/ui_util.h"
#import "ios/components/credential_provider_extension/password_spec_fetcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::GeneratePassword;
using autofill::PasswordRequirementsSpec;
using base::SysUTF16ToNSString;

@interface NewPasswordMediator ()

// The service identifier the password is being created for,
@property(nonatomic, strong) ASCredentialServiceIdentifier* serviceIdentifier;

// The NSUserDefaults new credentials should be stored to.
@property(nonatomic, strong) NSUserDefaults* userDefaults;

// Fetcher for password specs.
@property(nonatomic, strong) PasswordSpecFetcher* fetcher;

@end

@implementation NewPasswordMediator

- (instancetype)initWithUserDefaults:(NSUserDefaults*)userDefaults
                   serviceIdentifier:
                       (ASCredentialServiceIdentifier*)serviceIdentifier {
  self = [super init];
  if (self) {
    _userDefaults = userDefaults;
    _serviceIdentifier = serviceIdentifier;
    NSString* host = HostForServiceIdentifier(serviceIdentifier);
    _fetcher =
        [[PasswordSpecFetcher alloc] initWithHost:host
                                           APIKey:BUILDFLAG(GOOGLE_API_KEY)];
    [_fetcher fetchSpecWithCompletion:nil];
  }
  return self;
}

#pragma mark - NewCredentialHandler

- (void)userDidRequestGeneratedPassword {
  if (self.fetcher.didFetchSpec) {
    PasswordRequirementsSpec spec = self.fetcher.spec;
    [self.uiHandler setPassword:SysUTF16ToNSString(GeneratePassword(spec))];
    return;
  }
  __weak __typeof__(self) weakSelf = self;
  [self.fetcher fetchSpecWithCompletion:^(PasswordRequirementsSpec spec) {
    [weakSelf.uiHandler setPassword:SysUTF16ToNSString(GeneratePassword(spec))];
  }];
}

- (void)saveCredentialWithUsername:(NSString*)username
                          password:(NSString*)password
                     shouldReplace:(BOOL)shouldReplace {
  if (!shouldReplace && [self credentialExistsForUsername:username]) {
    [self.uiHandler alertUserCredentialExists];
    return;
  }

  ArchivableCredential* credential =
      [self createNewCredentialWithUsername:username password:password];

  if (!credential) {
    [self.uiHandler alertSavePasswordFailed];
    return;
  }

  [self
      saveNewCredential:credential
             completion:^(NSError* error) {
               if (error) {
                 UpdateUMACountForKey(
                     app_group::kCredentialExtensionSaveCredentialFailureCount);
                 [self.uiHandler alertSavePasswordFailed];
                 return;
               }
               [self.uiHandler credentialSaved:credential];
               [self userSelectedCredential:credential];
             }];
}

#pragma mark - Private

// Checks whether a credential already exists with the given username.
- (BOOL)credentialExistsForUsername:(NSString*)username {
  NSURL* url = [NSURL URLWithString:[self currentIdentifier]];
  NSString* recordIdentifier = RecordIdentifierForData(url, username);

  return [self.existingCredentials
      credentialWithRecordIdentifier:recordIdentifier];
}

// Creates a new credential but doesn't add it to any stores.
- (ArchivableCredential*)createNewCredentialWithUsername:(NSString*)username
                                                password:(NSString*)password {
  NSString* identifier = [self currentIdentifier];
  NSURL* url = [NSURL URLWithString:identifier];
  NSString* recordIdentifier = RecordIdentifierForData(url, username);

  NSString* uuid = [[NSUUID UUID] UUIDString];
  if (!StorePasswordInKeychain(password, uuid)) {
    return nil;
  }
  NSString* validationIdentifierKey =
      AppGroupUserDefaultsCredentialProviderUserID();
  NSString* validationIdentifier =
      [app_group::GetGroupUserDefaults() stringForKey:validationIdentifierKey];

  return [[ArchivableCredential alloc] initWithFavicon:nil
                                    keychainIdentifier:uuid
                                                  rank:1
                                      recordIdentifier:recordIdentifier
                                     serviceIdentifier:identifier
                                           serviceName:url.host ?: identifier
                                                  user:username
                                  validationIdentifier:validationIdentifier];
}

// Saves the given credential to disk and calls `completion` once the operation
// is finished.
- (void)saveNewCredential:(ArchivableCredential*)credential
               completion:(void (^)(NSError* error))completion {
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
      initWithUserDefaults:self.userDefaults
                       key:key];

  if ([store credentialWithRecordIdentifier:credential.recordIdentifier]) {
    [store updateCredential:credential];
  } else {
    [store addCredential:credential];
  }

  [store saveDataWithCompletion:completion];
}

// Alerts the host app that the user selected a credential.
- (void)userSelectedCredential:(id<Credential>)credential {
  NSString* password =
      PasswordWithKeychainIdentifier(credential.keychainIdentifier);
  ASPasswordCredential* ASCredential =
      [ASPasswordCredential credentialWithUser:credential.user
                                      password:password];
  [self.context completeRequestWithSelectedCredential:ASCredential
                                    completionHandler:nil];
}

- (NSString*)currentIdentifier {
  NSString* identifier = self.serviceIdentifier.identifier;

  // According to Apple
  // (https://developer.apple.com/documentation/xcode/supporting-associated-domains).
  // associated domains must have an https:// scheme, and to autofill passwords
  // an associated domain is needed
  // (https://developer.apple.com/documentation/security/password_autofill/).
  // Also iOS strips https:// from passed identifier, Chrome restores it here to
  // save a valid URL.
  if (self.serviceIdentifier.type == ASCredentialServiceIdentifierTypeDomain &&
      ![identifier hasPrefix:@"https://"]) {
    identifier = [@"https://" stringByAppendingString:identifier];
  }
  return identifier;
}

@end
