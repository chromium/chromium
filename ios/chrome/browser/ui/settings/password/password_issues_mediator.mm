// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"

#import "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/password_manager_util_ios.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordIssuesMediator () <PasswordCheckObserver> {
  IOSChromePasswordCheckManager* _manager;

  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  std::vector<password_manager::CredentialUIEntry> _insecureCredentials;
}

// Object storing the time of the previous successful re-authentication.
// This is meant to be used by the `ReauthenticationModule` for keeping
// re-authentications valid for a certain time interval within the scope
// of the Password Issues Screen.
@property(nonatomic, strong, readonly) NSDate* successfulReauthTime;

// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

// Service to know whether passwords are synced.
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

@implementation PasswordIssuesMediator

- (instancetype)initWithPasswordCheckManager:
                    (IOSChromePasswordCheckManager*)manager
                               faviconLoader:(FaviconLoader*)faviconLoader
                                 syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    _syncService = syncService;
    _faviconLoader = faviconLoader;
    _manager = manager;
    _passwordCheckObserver.reset(
        new PasswordCheckObserverBridge(self, manager));
  }
  return self;
}

- (void)setConsumer:(id<PasswordIssuesConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self fetchPasswordIssues];
}

- (void)deleteCredential:
    (const password_manager::CredentialUIEntry&)credential {
  _manager->GetSavedPasswordsPresenter()->RemoveCredential(credential);
  // TODO:(crbug.com/1075494) - Update list of compromised passwords without
  // awaiting compromisedCredentialsDidChange.
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  // No-op.
}

- (void)compromisedCredentialsDidChange {
  [self fetchPasswordIssues];
}

#pragma mark - Private Methods

- (void)fetchPasswordIssues {
  DCHECK(self.consumer);
  _insecureCredentials = _manager->GetInsecureCredentials();
  NSMutableArray* passwords = [[NSMutableArray alloc] init];
  for (auto credential : _insecureCredentials) {
    [passwords addObject:[[PasswordIssue alloc] initWithCredential:credential]];
  }

  NSSortDescriptor* origin = [[NSSortDescriptor alloc] initWithKey:@"website"
                                                         ascending:YES];
  NSSortDescriptor* username = [[NSSortDescriptor alloc] initWithKey:@"username"
                                                           ascending:YES];

  [self.consumer
      setPasswordIssues:[passwords
                            sortedArrayUsingDescriptors:@[ origin, username ]]];
}

#pragma mark SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  _successfulReauthTime = [[NSDate alloc] init];
}

- (NSDate*)lastSuccessfulReauthTime {
  return [self successfulReauthTime];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForURL:(CrURL*)URL
           completion:(void (^)(FaviconAttributes*))completion {
  syncer::SyncService* syncService = self.syncService;
  BOOL isPasswordSyncEnabled =
      password_manager_util::IsPasswordSyncNormalEncryptionEnabled(syncService);
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/isPasswordSyncEnabled, completion);
}

@end
