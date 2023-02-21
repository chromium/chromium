// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"

#import <memory>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/password_manager_util_ios.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordIssuesMediator () <PasswordCheckObserver,
                                      SavedPasswordsPresenterObserver> {
  raw_ptr<IOSChromePasswordCheckManager> _manager;

  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _passwordsPresenterObserver;

  std::vector<password_manager::CredentialUIEntry> _insecureCredentials;

  // Object storing the time of the previous successful re-authentication.
  // This is meant to be used by the `ReauthenticationModule` for keeping
  // re-authentications valid for a certain time interval within the scope
  // of the Password Issues Screen.
  __strong NSDate* _successfulReauthTime;

  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Service to know whether passwords are synced.
  raw_ptr<syncer::SyncService> _syncService;
}

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
    _passwordCheckObserver =
        std::make_unique<PasswordCheckObserverBridge>(self, manager);
    _passwordsPresenterObserver =
        std::make_unique<SavedPasswordsPresenterObserverBridge>(
            self, _manager->GetSavedPasswordsPresenter());
  }
  return self;
}

- (void)disconnect {
  _passwordCheckObserver.reset();
  _passwordsPresenterObserver.reset();

  _manager = nullptr;
  _faviconLoader = nullptr;
  _syncService = nullptr;
}

- (void)setConsumer:(id<PasswordIssuesConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self providePasswordsToConsumer];
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  // No-op.
}

- (void)insecureCredentialsDidChange {
  [self providePasswordsToConsumer];
}

#pragma mark - SavedPasswordsPresenterObserver

- (void)savedPasswordsDidChange {
  [self providePasswordsToConsumer];
}

#pragma mark - Private Methods

- (void)providePasswordsToConsumer {
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
  return _successfulReauthTime;
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForURL:(CrURL*)URL
           completion:(void (^)(FaviconAttributes*))completion {
  BOOL isPasswordSyncEnabled =
      password_manager_util::IsPasswordSyncNormalEncryptionEnabled(
          _syncService);
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/isPasswordSyncEnabled, completion);
}

@end
