// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_mediator+Testing.h"

#import "base/memory/raw_ptr.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/sync/base/passphrase_enums.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/passwords/model/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/passwords/model/password_manager_util_ios.h"
#import "ios/chrome/browser/passwords/model/save_passwords_consumer.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/account_storage_utils.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using password_manager::WarningType;

namespace {

// Struct used to count and store the number of active Password Manager widget
// promos, as the FET does not support showing multiple promos for the same FET
// feature at the same time.
struct PasswordManagerActiveWidgetPromoData
    : public base::SupportsUserData::Data {
  // The number of active promos.
  int active_promos = 0;

  // Key to use for this type in SupportsUserData
  static constexpr char key[] = "PasswordManagerActiveWidgetPromoData";
};

}  // namespace

@interface PasswordsMediator () <PasswordCheckObserver,
                                 SavedPasswordsPresenterObserver,
                                 SyncObserverModelBridge>

// Whether or not the Feature Engagement Tracker should be notified that the
// Password Manager widget promo is not displayed anymore. Will be `true` when
// the Password Manager view controller is dismissed while presenting the
// promo.
@property(nonatomic, assign)
    BOOL shouldNotifyFETToDismissPasswordManagerWidgetPromo;

@end

@implementation PasswordsMediator {
  // The service responsible for password check feature.
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;

  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // A helper object for passing data about changes in password check status
  // and changes to compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordManagerViewController.
  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _passwordsPresenterObserver;

  // Current state of password check.
  PasswordCheckState _currentState;

  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;

  // Object storing the time of the previous successful re-authentication.
  // This is meant to be used by the `ReauthenticationModule` for keeping
  // re-authentications valid for a certain time interval within the scope
  // of the Passwords Screen.
  __strong NSDate* _successfulReauthTime;

  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Service to know whether passwords are synced.
  raw_ptr<syncer::SyncService> _syncService;

  // The user pref service.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPasswordCheckManager:
                    (scoped_refptr<IOSChromePasswordCheckManager>)
                        passwordCheckManager
                               faviconLoader:(FaviconLoader*)faviconLoader
                                 syncService:(syncer::SyncService*)syncService
                                 prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _syncService = syncService;
    _prefService = prefService;
    _faviconLoader = faviconLoader;

    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);

    _passwordCheckManager = passwordCheckManager;
    _savedPasswordsPresenter =
        passwordCheckManager->GetSavedPasswordsPresenter();
    DCHECK(_savedPasswordsPresenter);

    _passwordCheckObserver = std::make_unique<PasswordCheckObserverBridge>(
        self, _passwordCheckManager.get());
    _passwordsPresenterObserver =
        std::make_unique<SavedPasswordsPresenterObserverBridge>(
            self, _savedPasswordsPresenter);
  }
  return self;
}

- (void)setConsumer:(id<PasswordsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  [self providePasswordsToConsumer];

  _currentState = _passwordCheckManager->GetPasswordCheckState();
  [self updateConsumerPasswordCheckState:_currentState];
  [self.consumer
      setSavingPasswordsToAccount:
          password_manager::sync_util::GetPasswordSyncState(_syncService) !=
          password_manager::sync_util::SyncState::kNotActive];
}

- (void)disconnect {
  if (_shouldNotifyFETToDismissPasswordManagerWidgetPromo && _tracker) {
    [self dismissFETIfNeeded];
  }
  _tracker = nullptr;
  _syncObserver.reset();
  _passwordsPresenterObserver.reset();
  _passwordCheckObserver.reset();
  _passwordCheckManager.reset();
  _savedPasswordsPresenter = nullptr;
  _faviconLoader = nullptr;
  _prefService = nullptr;
  _syncService = nullptr;
}

- (void)askFETToShowPasswordManagerWidgetPromo {
  if (self.tracker && !_shouldNotifyFETToDismissPasswordManagerWidgetPromo) {
    [self.consumer setShouldShowPasswordManagerWidgetPromo:
                       [self shouldShowPasswordManagerWidgetPromo]];
  }
}

#pragma mark - PasswordManagerViewControllerDelegate

- (void)deleteCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials {
  for (const auto& credential : credentials) {
    _savedPasswordsPresenter->RemoveCredential(credential);
  }
}

- (void)startPasswordCheck {
  _passwordCheckManager->StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kBulkSyncedPasswordsCheck);
}

- (NSString*)formattedElapsedTimeSinceLastCheck {
  std::optional<base::Time> lastCompletedCheck =
      _passwordCheckManager->GetLastPasswordCheckTime();
  return password_manager::FormatElapsedTimeSinceLastCheck(lastCompletedCheck);
}

- (NSAttributedString*)passwordCheckErrorInfo {
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
      message =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_SIGNED_OUT);
      break;
    case PasswordCheckState::kOffline:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_OFFLINE);
      break;
    case PasswordCheckState::kQuotaLimit:
      if ([self canUseAccountPasswordCheckup]) {
        message = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_CHECKUP_ERROR_QUOTA_LIMIT_VISIT_GOOGLE);
        NSDictionary* linkAttributes = @{
          NSLinkAttributeName :
              net::NSURLWithGURL(password_manager::GetPasswordCheckupURL(
                  password_manager::PasswordCheckupReferrer::kPasswordCheck))
        };

        return AttributedStringFromStringWithLink(message, textAttributes,
                                                  linkAttributes);
      } else {
        message =
            l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_QUOTA_LIMIT);
      }
      break;
    case PasswordCheckState::kOther:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_OTHER);
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

- (BOOL)shouldShowLocalOnlyIconForCredential:
    (const password_manager::CredentialUIEntry&)credential {
  return password_manager::ShouldShowLocalOnlyIcon(credential, _syncService);
}

- (BOOL)shouldShowLocalOnlyIconForGroup:
    (const password_manager::AffiliatedGroup&)group {
  return password_manager::ShouldShowLocalOnlyIconForGroup(group, _syncService);
}

- (void)notifyFETOfPasswordManagerWidgetPromoDismissal {
  if (self.tracker) {
    self.tracker->NotifyEvent(
        feature_engagement::events::kPasswordManagerWidgetPromoClosed);
    [self dismissFETIfNeeded];
  }
  _shouldNotifyFETToDismissPasswordManagerWidgetPromo = NO;
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  if (state == _currentState)
    return;

  [self updateConsumerPasswordCheckState:state];
}

- (void)insecureCredentialsDidChange {
  // Insecure password changes have no effect on UI while check is running.
  if (_passwordCheckManager->GetPasswordCheckState() ==
      PasswordCheckState::kRunning)
    return;

  [self updateConsumerPasswordCheckState:_currentState];
}

- (void)passwordCheckManagerWillShutdown {
  _passwordCheckObserver.reset();
}

#pragma mark - Private Methods

// Provides passwords and blocked forms to the '_consumer'.
- (void)providePasswordsToConsumer {
  [_consumer setAffiliatedGroups:_savedPasswordsPresenter->GetAffiliatedGroups()
                    blockedSites:_savedPasswordsPresenter->GetBlockedSites()];
}

// Updates the `_consumer` Password Check UI State and Insecure Passwords.
- (void)updateConsumerPasswordCheckState:
    (PasswordCheckState)passwordCheckState {
  DCHECK(self.consumer);
  std::vector<password_manager::CredentialUIEntry> insecureCredentials =
      _passwordCheckManager->GetInsecureCredentials();
  PasswordCheckUIState passwordCheckUIState =
      [self computePasswordCheckUIStateWith:passwordCheckState
                        insecureCredentials:insecureCredentials];
  WarningType warningType = GetWarningOfHighestPriority(insecureCredentials);
  NSInteger insecurePasswordsCount =
      GetPasswordCountForWarningType(warningType, insecureCredentials);
  [self.consumer setPasswordCheckUIState:passwordCheckUIState
                  insecurePasswordsCount:insecurePasswordsCount];
}

// Returns PasswordCheckUIState based on PasswordCheckState.
- (PasswordCheckUIState)
    computePasswordCheckUIStateWith:(PasswordCheckState)newState
                insecureCredentials:
                    (const std::vector<password_manager::CredentialUIEntry>&)
                        insecureCredentials {
  BOOL wasRunning = _currentState == PasswordCheckState::kRunning;
  _currentState = newState;

  switch (_currentState) {
    case PasswordCheckState::kRunning:
      return PasswordCheckStateRunning;
    case PasswordCheckState::kNoPasswords:
      return PasswordCheckStateDisabled;
    case PasswordCheckState::kSignedOut:
      return PasswordCheckStateSignedOut;
    case PasswordCheckState::kOffline:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther:
      return PasswordCheckStateError;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle: {
      if (_currentState == PasswordCheckState::kIdle && wasRunning) {
        PasswordCheckUIState insecureState =
            [self passwordCheckUIStateFromHighestPriorityWarningType:
                      insecureCredentials];
        return insecureCredentials.empty() ? PasswordCheckStateSafe
                                           : insecureState;
      }
      return PasswordCheckStateDefault;
    }
  }
}

// Returns the right PasswordCheckUIState depending on the highest priority
// warning type.
- (PasswordCheckUIState)passwordCheckUIStateFromHighestPriorityWarningType:
    (const std::vector<password_manager::CredentialUIEntry>&)
        insecureCredentials {
  switch (GetWarningOfHighestPriority(insecureCredentials)) {
    case WarningType::kCompromisedPasswordsWarning:
      return PasswordCheckStateUnmutedCompromisedPasswords;
    case WarningType::kReusedPasswordsWarning:
      return PasswordCheckStateReusedPasswords;
    case WarningType::kWeakPasswordsWarning:
      return PasswordCheckStateWeakPasswords;
    case WarningType::kDismissedWarningsWarning:
      return PasswordCheckStateDismissedWarnings;
    case WarningType::kNoInsecurePasswordsWarning:
      return PasswordCheckStateSafe;
  }
}

// Compute whether user is capable to run password check in Google Account.
- (BOOL)canUseAccountPasswordCheckup {
  return password_manager::sync_util::GetAccountForSaving(_prefService,
                                                          _syncService) &&
         !_syncService->GetUserSettings()->IsEncryptEverythingEnabled();
}

- (BOOL)shouldShowPasswordManagerWidgetPromo {
  if (self.tracker) {
    // First check if another active Password Manager page (e.g. in another
    // window) has an active promo. If so, just return that the promo should be
    // shown here without querying the FET. Only query the FET if there is no
    // currently active promo.
    PasswordManagerActiveWidgetPromoData* data =
        static_cast<PasswordManagerActiveWidgetPromoData*>(
            self.tracker->GetUserData(
                PasswordManagerActiveWidgetPromoData::key));
    if (data) {
      data->active_promos++;
      self.shouldNotifyFETToDismissPasswordManagerWidgetPromo = YES;
      return YES;
    } else if (self.tracker->ShouldTriggerHelpUI(
                   feature_engagement::
                       kIPHiOSPromoPasswordManagerWidgetFeature)) {
      std::unique_ptr<PasswordManagerActiveWidgetPromoData> new_data =
          std::make_unique<PasswordManagerActiveWidgetPromoData>();
      new_data->active_promos++;
      self.tracker->SetUserData(PasswordManagerActiveWidgetPromoData::key,
                                std::move(new_data));
      self.shouldNotifyFETToDismissPasswordManagerWidgetPromo = YES;
      return YES;
    }
  }
  return NO;
}

// Check if this is the last active Password Manager showing the widget promo
// and dismisses the FET if so.
- (void)dismissFETIfNeeded {
  PasswordManagerActiveWidgetPromoData* data =
      static_cast<PasswordManagerActiveWidgetPromoData*>(
          _tracker->GetUserData(PasswordManagerActiveWidgetPromoData::key));
  if (data) {
    data->active_promos--;
    if (data->active_promos <= 0) {
      _tracker->Dismissed(
          feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature);
      _tracker->RemoveUserData(PasswordManagerActiveWidgetPromoData::key);
    }
  } else {
    _tracker->Dismissed(
        feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature);
  }
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
  return _successfulReauthTime;
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  BOOL fallbackToGoogleServer =
      password_manager_util::IsSavingPasswordsToAccountWithNormalEncryption(
          _syncService);
  _faviconLoader->FaviconForPageUrl(URL.gurl, kDesiredMediumFaviconSizePt,
                                    kMinFaviconSizePt, fallbackToGoogleServer,
                                    completion);
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self.consumer
      setSavingPasswordsToAccount:
          password_manager::sync_util::GetPasswordSyncState(_syncService) !=
          password_manager::sync_util::SyncState::kNotActive];
}

@end
