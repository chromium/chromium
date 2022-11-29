// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/driver/sync_service_utils.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/password_manager_util_ios.h"
#import "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Amount of time after which timestamp is shown instead of "just now".
constexpr base::TimeDelta kJustCheckedTimeThresholdInMinutes = base::Minutes(1);
}  // namespace

@interface PasswordsMediator () <IdentityManagerObserverBridgeDelegate,
                                 PasswordCheckObserver,
                                 SavedPasswordsPresenterObserver,
                                 SyncObserverModelBridge> {
  // The service responsible for password check feature.
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;

  // Service to check if passwords are synced.
  SyncSetupService* _syncSetupService;

  password_manager::SavedPasswordsPresenter* _savedPasswordsPresenter;

  // A helper object for passing data about changes in password check status
  // and changes to compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordManagerViewController.
  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _passwordsPresenterObserver;

  // Current state of password check.
  PasswordCheckState _currentState;

  // IdentityManager observer.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Sync observer
  std::unique_ptr<SyncObserverBridge> _syncObserver;
}

// Object storing the time of the previous successful re-authentication.
// This is meant to be used by the `ReauthenticationModule` for keeping
// re-authentications valid for a certain time interval within the scope
// of the Passwords Screen.
@property(nonatomic, strong, readonly) NSDate* successfulReauthTime;

// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

// Service to know whether passwords are synced.
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

@implementation PasswordsMediator

- (instancetype)
    initWithPasswordCheckManager:
        (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager
                syncSetupService:(SyncSetupService*)syncSetupService
                   faviconLoader:(FaviconLoader*)faviconLoader
                 identityManager:(signin::IdentityManager*)identityManager
                     syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    _syncService = syncService;
    _faviconLoader = faviconLoader;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);

    _syncSetupService = syncSetupService;

    _passwordCheckManager = passwordCheckManager;
    _savedPasswordsPresenter =
        passwordCheckManager->GetSavedPasswordsPresenter();
    DCHECK(_savedPasswordsPresenter);

    _passwordCheckObserver = std::make_unique<PasswordCheckObserverBridge>(
        self, _passwordCheckManager.get());
    _passwordsPresenterObserver =
        std::make_unique<SavedPasswordsPresenterObserverBridge>(
            self, _savedPasswordsPresenter);
    [[PasswordAutoFillStatusManager sharedManager] addObserver:self];
  }
  return self;
}

- (void)dealloc {
  if (_passwordsPresenterObserver) {
    _savedPasswordsPresenter->RemoveObserver(_passwordsPresenterObserver.get());
  }
  if (_passwordCheckObserver) {
    _passwordCheckManager->RemoveObserver(_passwordCheckObserver.get());
  }
  [[PasswordAutoFillStatusManager sharedManager] removeObserver:self];
}

- (void)setConsumer:(id<PasswordsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  [self providePasswordsToConsumer];

  _currentState = _passwordCheckManager->GetPasswordCheckState();
  [self updateConsumerPasswordCheckState:_currentState];
}

- (void)deleteCredential:
    (const password_manager::CredentialUIEntry&)credential {
  _savedPasswordsPresenter->RemoveCredential(credential);
}

- (void)disconnect {
  _identityManagerObserver.reset();
  _syncObserver.reset();
  _syncService = nullptr;
}

#pragma mark - PasswordManagerViewControllerDelegate

- (void)deleteCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials {
  for (const auto& credential : credentials) {
    _savedPasswordsPresenter->RemoveCredential(credential);
  }
}

- (void)startPasswordCheck {
  _passwordCheckManager->StartPasswordCheck();
}

- (NSString*)formatElapsedTimeSinceLastCheck {
  base::Time lastCompletedCheck =
      _passwordCheckManager->GetLastPasswordCheckTime();

  // lastCompletedCheck is 0.0 in case the check never completely ran before.
  if (lastCompletedCheck == base::Time())
    return l10n_util::GetNSString(IDS_IOS_CHECK_NEVER_RUN);

  base::TimeDelta elapsedTime = base::Time::Now() - lastCompletedCheck;

  NSString* timestamp;
  // If check finished in less than `kJustCheckedTimeThresholdInMinutes` show
  // "just now" instead of timestamp.
  if (elapsedTime < kJustCheckedTimeThresholdInMinutes)
    timestamp = l10n_util::GetNSString(IDS_IOS_CHECK_FINISHED_JUST_NOW);
  else
    timestamp = base::SysUTF8ToNSString(
        base::UTF16ToUTF8(ui::TimeFormat::SimpleWithMonthAndYear(
            ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
            elapsedTime, true)));

  return l10n_util::GetNSStringF(IDS_IOS_LAST_COMPLETED_CHECK,
                                 base::SysNSStringToUTF16(timestamp));
}

- (NSAttributedString*)passwordCheckErrorInfo {
  if (!_passwordCheckManager->GetUnmutedCompromisedCredentials().empty())
    return nil;

  NSString* message;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  switch (_currentState) {
    case PasswordCheckState::kRunning:
    case PasswordCheckState::kNoPasswords:
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle:
      return nil;
    case PasswordCheckState::kSignedOut:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_SIGNED_OUT);
      break;
    case PasswordCheckState::kOffline:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_OFFLINE);
      break;
    case PasswordCheckState::kQuotaLimit:
      if ([self canUseAccountPasswordCheckup]) {
        message = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_CHECK_ERROR_QUOTA_LIMIT_VISIT_GOOGLE);
        NSDictionary* linkAttributes = @{
          NSLinkAttributeName :
              net::NSURLWithGURL(password_manager::GetPasswordCheckupURL(
                  password_manager::PasswordCheckupReferrer::kPasswordCheck))
        };

        return AttributedStringFromStringWithLink(message, textAttributes,
                                                  linkAttributes);
      } else {
        message =
            l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_QUOTA_LIMIT);
      }
      break;
    case PasswordCheckState::kOther:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_OTHER);
      break;
  }
  return [[NSMutableAttributedString alloc] initWithString:message
                                                attributes:textAttributes];
}

// Returns the on-device encryption state according to the sync service.
- (OnDeviceEncryptionState)onDeviceEncryptionState {
  if (ShouldOfferTrustedVaultOptIn(_syncService)) {
    return OnDeviceEncryptionStateOfferOptIn;
  }
  syncer::SyncUserSettings* syncUserSettings = _syncService->GetUserSettings();
  if (syncUserSettings->GetPassphraseType() ==
      syncer::PassphraseType::kTrustedVaultPassphrase) {
    return OnDeviceEncryptionStateOptedIn;
  }
  return OnDeviceEncryptionStateNotShown;
}

- (BOOL)isSyncingPasswords {
  return password_manager_util::GetPasswordSyncState(_syncService) !=
         password_manager::SyncState::kNotSyncing;
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  if (state == _currentState)
    return;

  [self updateConsumerPasswordCheckState:state];
}

- (void)compromisedCredentialsDidChange {
  // Compromised passwords changes has no effect on UI while check is running.
  if (_passwordCheckManager->GetPasswordCheckState() ==
      PasswordCheckState::kRunning)
    return;

  [self updateConsumerPasswordCheckState:_currentState];
}

#pragma mark - PasswordAutoFillStatusObserver

- (void)passwordAutoFillStatusDidChange {
  // Since this action is appended to the main queue, at this stage,
  // self.consumer should have already been setup.
  DCHECK(self.consumer);
  [self.consumer updatePasswordsInOtherAppsDetailedText];
}

#pragma mark - Private Methods

// Provides passwords and blocked forms to the '_consumer'.
- (void)providePasswordsToConsumer {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordsGrouping)) {
    [_consumer
        setAffiliatedGroups:_savedPasswordsPresenter->GetAffiliatedGroups()
               blockedSites:_savedPasswordsPresenter->GetBlockedSites()];
  } else {
    std::vector<password_manager::CredentialUIEntry> passwords, blockedSites;
    for (const auto& credential :
         _savedPasswordsPresenter->GetSavedCredentials()) {
      if (credential.blocked_by_user) {
        blockedSites.push_back(std::move(credential));
      } else {
        passwords.push_back(std::move(credential));
      }
    }
    [_consumer setPasswords:std::move(passwords)
               blockedSites:std::move(blockedSites)];
  }
}

// Updates the `_consumer` Password Check UI State and Unmuted Compromised
// Passwords.
- (void)updateConsumerPasswordCheckState:
    (PasswordCheckState)passwordCheckState {
  DCHECK(self.consumer);

  PasswordCheckUIState passwordCheckUIState =
      [self computePasswordCheckUIStateWith:passwordCheckState];
  NSInteger unmutedCompromisedPasswordsCount =
      _passwordCheckManager->GetUnmutedCompromisedCredentials().size();
  [self.consumer setPasswordCheckUIState:passwordCheckUIState
        unmutedCompromisedPasswordsCount:unmutedCompromisedPasswordsCount];
}

// Returns PasswordCheckUIState based on PasswordCheckState.
- (PasswordCheckUIState)computePasswordCheckUIStateWith:
    (PasswordCheckState)newState {
  BOOL wasRunning = _currentState == PasswordCheckState::kRunning;
  _currentState = newState;

  switch (_currentState) {
    case PasswordCheckState::kRunning:
      return PasswordCheckStateRunning;
    case PasswordCheckState::kNoPasswords:
      return PasswordCheckStateDisabled;
    case PasswordCheckState::kSignedOut:
    case PasswordCheckState::kOffline:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther:
      return _passwordCheckManager->GetUnmutedCompromisedCredentials().empty()
                 ? PasswordCheckStateError
                 : PasswordCheckStateUnSafe;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle: {
      if (!_passwordCheckManager->GetUnmutedCompromisedCredentials().empty()) {
        return PasswordCheckStateUnSafe;
      } else if (_currentState == PasswordCheckState::kIdle) {
        // Safe state is only possible after the state transitioned from
        // kRunning to kIdle.
        return wasRunning ? PasswordCheckStateSafe : PasswordCheckStateDefault;
      }
      return PasswordCheckStateDefault;
    }
  }
}

// Compute whether user is capable to run password check in Google Account.
- (BOOL)canUseAccountPasswordCheckup {
  return _syncSetupService->CanSyncFeatureStart() &&
         !_syncSetupService->IsEncryptEverythingEnabled();
}

#pragma mark - SavedPasswordsPresenterObserver

- (void)savedPasswordsDidChange {
  [self providePasswordsToConsumer];
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

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self.consumer updateOnDeviceEncryptionSessionAndUpdateTableView];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self.consumer updateOnDeviceEncryptionSessionAndUpdateTableView];
}

@end
