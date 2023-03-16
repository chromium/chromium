// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_mediator.h"

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
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::CredentialUIEntry;
using password_manager::WarningType;
using password_manager::features::IsPasswordCheckupEnabled;

namespace {

// Maps CredentialUIEntry to PasswordIssue and sorts them by website and
// username.
NSArray<PasswordIssue*>* GetSortedPasswordIssues(
    const std::vector<CredentialUIEntry>& insecure_credentials) {
  NSMutableArray<PasswordIssue*>* passwords = [[NSMutableArray alloc] init];
  for (auto credential : insecure_credentials) {
    [passwords addObject:[[PasswordIssue alloc] initWithCredential:credential]];
  }

  NSSortDescriptor* origin = [[NSSortDescriptor alloc] initWithKey:@"website"
                                                         ascending:YES];
  NSSortDescriptor* username = [[NSSortDescriptor alloc] initWithKey:@"username"
                                                           ascending:YES];

  return [passwords sortedArrayUsingDescriptors:@[ origin, username ]];
}

// Creates a `PasswordIssueGroup` for each set of issues with the same password.
// Issues in the same group are sorted by their position in `password_issues`.
// Groups are sorted by the position of their first issue in `password_issues`.
NSArray<PasswordIssueGroup*>* GroupIssuesByPassword(
    NSArray<PasswordIssue*>* password_issues) {
  // Holds issues with the same password.
  // Used for tracking the order of each group.
  NSMutableArray<NSMutableArray<PasswordIssue*>*>* same_password_issues =
      [NSMutableArray array];
  // Used for grouping issues by passsword.
  NSMutableDictionary<NSString*, NSMutableArray*>* issue_groups =
      [NSMutableDictionary dictionary];

  for (PasswordIssue* issue in password_issues) {
    NSString* password = base::SysUTF16ToNSString(issue.credential.password);

    NSMutableArray<PasswordIssue*>* issues_in_group =
        [issue_groups objectForKey:password];
    // Add issue to existing group with same password.
    if (issues_in_group) {
      [issues_in_group addObject:issue];
    } else {
      // First issue with this password, add it to its own group.
      issues_in_group = [NSMutableArray arrayWithObject:issue];
      [same_password_issues addObject:issues_in_group];
      issue_groups[password] = issues_in_group;
    }
  }

  // Map issue groups to PasswordIssueGroups.
  NSMutableArray<PasswordIssueGroup*>* password_issue_groups =
      [NSMutableArray arrayWithCapacity:same_password_issues.count];
  [same_password_issues
      enumerateObjectsUsingBlock:^(NSMutableArray<PasswordIssue*>* issues,
                                   NSUInteger index, BOOL* stop) {
        // TODO(crbug.com/1406540): Add header for each group.
        [password_issue_groups
            addObject:[[PasswordIssueGroup alloc] initWithHeaderText:nil
                                                      passwordIssues:issues]];
      }];

  return password_issue_groups;
}

// Maps CredentialUIEntry to PasswordIssue sorted and grouped according to their
// `warning_type`.
NSArray<PasswordIssueGroup*>* GetPasswordIssueGroups(
    WarningType warning_type,
    const std::vector<CredentialUIEntry>& insecure_credentials) {
  if (insecure_credentials.empty()) {
    return @[];
  }

  // Sort by website and username.
  NSArray<PasswordIssue*>* sorted_issues =
      GetSortedPasswordIssues(insecure_credentials);

  // Reused issues are grouped by passwords.
  if (warning_type == WarningType::kReusedPasswordsWarning) {
    return GroupIssuesByPassword(sorted_issues);
  } else {
    // Other types are all displayed in the same group without header.
    return @[ [[PasswordIssueGroup alloc] initWithHeaderText:nil
                                              passwordIssues:sorted_issues] ];
  }
}

}  // namespace

@interface PasswordIssuesMediator () <PasswordCheckObserver,
                                      SavedPasswordsPresenterObserver> {
  WarningType _warningType;

  raw_ptr<IOSChromePasswordCheckManager> _manager;

  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _passwordsPresenterObserver;

  // Last set of insecure credentials provided to the consumer. Used to avoid
  // updating the UI when changes in the insecure credentials happen but the
  // credentials provided to the consumer don't (e.g a new compromised
  // credential was detected but the consumer is displaying weak credentials).
  // A value of nullopt means the consumer hasn't been provided with credentials
  // yet.
  absl::optional<std::vector<CredentialUIEntry>> _insecureCredentials;

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
  if (_consumer == consumer) {
    return;
  }
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

  std::vector<CredentialUIEntry> insecureCredentials;
  if (IsPasswordCheckupEnabled()) {
    insecureCredentials = GetPasswordsForWarningType(
        _warningType, _manager->GetInsecureCredentials());

    if (_insecureCredentials.has_value() &&
        _insecureCredentials.value() == insecureCredentials) {
      // Avoid updating the UI when no changes occurred in the insecure
      // credentials for the warning type being displayed.
      return;
    }
    _insecureCredentials = insecureCredentials;
  } else {
    insecureCredentials = _manager->GetInsecureCredentials();
  }

  [self.consumer setPasswordIssues:GetPasswordIssueGroups(_warningType,
                                                          insecureCredentials)];

  [self.consumer
      setNavigationBarTitle:[self navigationBarTitleForNumberOfIssues:
                                      insecureCredentials.size()]];
}

// Computes the navigation bar title based on `_context`, number of issues and
// feature flags.
- (NSString*)navigationBarTitleForNumberOfIssues:(long)numberOfIssues {
  if (IsPasswordCheckupEnabled()) {
    switch (_warningType) {
      case WarningType::kWeakPasswordsWarning:
        return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
            IDS_IOS_WEAK_PASSWORD_ISSUES_TITLE, numberOfIssues));

      case WarningType::kCompromisedPasswordsWarning:
        return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
            IDS_IOS_COMPROMISED_PASSWORD_ISSUES_TITLE, numberOfIssues));

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
