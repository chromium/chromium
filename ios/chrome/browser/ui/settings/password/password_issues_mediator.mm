// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"

#import <memory>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/password_manager_util_ios.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns true if the Password Checkup feature flag is enabled.
bool IsPasswordCheckupEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kIOSPasswordCheckup);
}

}  // namespace

@interface PasswordIssuesMediator () <PasswordCheckObserver,
                                      SavedPasswordsPresenterObserver> {
  WarningType _warningType;

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

- (instancetype)initForWarningType:(WarningType)warningType
              passwordCheckManager:(IOSChromePasswordCheckManager*)manager
                     faviconLoader:(FaviconLoader*)faviconLoader
                       syncService:(syncer::SyncService*)syncService {
  DCHECK(manager);
  DCHECK(syncService);
  // `faviconLoader` might be null in tests.

  self = [super init];
  if (self) {
    _warningType = warningType;
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

  [self setConsumerHeader];

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

  // TODO:(crbug.com/1406540) - Filter credentials for the current context;

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

  [self.consumer
      setNavigationBarTitle:
          [self navigationBarTitleForNumberOfIssues:passwords.count]];
}

// Computes the navigation bar title based on `_context`, number of issues and
// feature flags.
- (NSString*)navigationBarTitleForNumberOfIssues:(NSUInteger)numberOfIssues {
  if (IsPasswordCheckupEnabled()) {
    switch (_warningType) {
      case WarningType::kWeakPasswordsWarning:
        return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
            IDS_IOS_WEAK_PASSWORD_ISSUES_TITLE, (long)numberOfIssues));

      case WarningType::kCompromisedPasswordsWarning:
        return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
            IDS_IOS_COMPROMISED_PASSWORD_ISSUES_TITLE, (long)numberOfIssues));

      case WarningType::kDismissedWarningsWarning:
        return l10n_util::GetNSString(
            IDS_IOS_DISMISSED_WARNINGS_PASSWORD_ISSUES_TITLE);

      case WarningType::kReusedPasswordsWarning:
        return l10n_util::GetNSStringF(IDS_IOS_REUSED_PASSWORD_ISSUES_TITLE,
                                       base::NumberToString16(numberOfIssues));

      case WarningType::kNoInsecurePasswordsWarning:
        // no-op
        return nil;
    }

  } else {
    return l10n_util::GetNSString(IDS_IOS_PASSWORDS);
  }
}

- (void)setConsumerHeader {
  int headerTextID;
  absl::optional<GURL> headerURL;

  if (IsPasswordCheckupEnabled()) {
    switch (_warningType) {
      case WarningType::kWeakPasswordsWarning:
        headerTextID = IDS_IOS_WEAK_PASSWORD_ISSUES_DESCRIPTION;
        headerURL =
            GURL(password_manager::
                     kPasswordManagerHelpCenterCreateStrongPasswordsURL);
        break;
      case WarningType::kCompromisedPasswordsWarning:
        headerTextID = IDS_IOS_COMPROMISED_PASSWORD_ISSUES_DESCRIPTION;
        headerURL =
            GURL(password_manager::
                     kPasswordManagerHelpCenterChangeUnsafePasswordsURL);
        break;
      case WarningType::kReusedPasswordsWarning:
        headerTextID = IDS_IOS_REUSED_PASSWORD_ISSUES_DESCRIPTION;
        headerURL = absl::nullopt;
        break;
      // Dismissed Warnings Page doesn't have a header.
      case WarningType::kDismissedWarningsWarning:
      case WarningType::kNoInsecurePasswordsWarning:
        // no-op
        return;
    }

  } else {
    headerTextID = IDS_IOS_PASSWORD_ISSUES_DESCRIPTION;
    headerURL = absl::nullopt;
  }

  NSString* headerText = l10n_util::GetNSString(headerTextID);
  CrURL* localizedHeaderURL =
      headerURL.has_value()
          ? [[CrURL alloc] initWithGURL:google_util::AppendGoogleLocaleParam(
                                            headerURL.value(),
                                            GetApplicationContext()
                                                ->GetApplicationLocale())]
          : nil;

  [self.consumer setHeader:headerText URL:localizedHeaderURL];
}

#pragma mark SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  _successfulReauthTime = [[NSDate alloc] init];
}

- (NSDate*)lastSuccessfulReauthTime {
  return _successfulReauthTime;
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  BOOL isPasswordSyncEnabled =
      password_manager_util::IsPasswordSyncNormalEncryptionEnabled(
          _syncService);
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/isPasswordSyncEnabled, completion);
}

@end
