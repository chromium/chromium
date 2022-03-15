// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/net/crurl.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/favicon/favicon_constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

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
  // password store request to the PasswordsTableViewController.
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
// This is meant to be used by the |ReauthenticationModule| for keeping
// re-authentications valid for a certain time interval within the scope
// of the Passwords Screen.
@property(nonatomic, strong, readonly) NSDate* successfulReauthTime;

// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

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
    if (base::FeatureList::IsEnabled(kCredentialProviderExtensionPromo)) {
      [[PasswordAutoFillStatusManager sharedManager] addObserver:self];
    }
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
  if (base::FeatureList::IsEnabled(kCredentialProviderExtensionPromo)) {
    [[PasswordAutoFillStatusManager sharedManager] removeObserver:self];
  }
}

- (void)setConsumer:(id<PasswordsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  [self providePasswordsToConsumer];

  _currentState = _passwordCheckManager->GetPasswordCheckState();
  [self.consumer
               setPasswordCheckUIState:
                   [self computePasswordCheckUIStateWith:_currentState]
      unmutedCompromisedPasswordsCount:_passwordCheckManager
                                           ->GetUnmutedCompromisedCredentials()
                                           .size()];
}

- (void)deletePasswordForm:(const password_manager::PasswordForm&)form {
  _savedPasswordsPresenter->RemovePassword(form);
}

- (void)disconnect {
  _identityManagerObserver.reset();
  _syncObserver.reset();
}

#pragma mark - PasswordsTableViewControllerDelegate

- (void)deletePasswordForms:
    (const std::vector<password_manager::PasswordForm>&)forms {
  for (const auto& form : forms) {
    _savedPasswordsPresenter->RemovePassword(form);
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
  // If check finished in less than |kJustCheckedTimeThresholdInMinutes| show
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

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  if (state == _currentState)
    return;

  DCHECK(self.consumer);
  [self.consumer
               setPasswordCheckUIState:
                   [self computePasswordCheckUIStateWith:state]
      unmutedCompromisedPasswordsCount:_passwordCheckManager
                                           ->GetUnmutedCompromisedCredentials()
                                           .size()];
}

- (void)compromisedCredentialsDidChange:
    (password_manager::InsecureCredentialsManager::CredentialsView)credentials {
  // Compromised passwords changes has no effect on UI while check is running.
  if (_passwordCheckManager->GetPasswordCheckState() ==
      PasswordCheckState::kRunning)
    return;

  DCHECK(self.consumer);

  [self.consumer setPasswordCheckUIState:
                     [self computePasswordCheckUIStateWith:_currentState]
        unmutedCompromisedPasswordsCount:credentials.size()];
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
  std::vector<password_manager::PasswordForm> forms =
      _savedPasswordsPresenter->GetUniquePasswordForms();

  std::vector<password_manager::PasswordForm> savedForms, blockedForms;
  for (const auto& form : forms) {
    if (form.blocked_by_user) {
      blockedForms.push_back(std::move(form));
    } else {
      savedForms.push_back(std::move(form));
    }
  }

  [_consumer setPasswordsForms:std::move(savedForms)
                  blockedForms:std::move(blockedForms)];
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

- (void)savedPasswordsDidChanged:
    (password_manager::SavedPasswordsPresenter::SavedPasswordsView)passwords {
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
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
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
