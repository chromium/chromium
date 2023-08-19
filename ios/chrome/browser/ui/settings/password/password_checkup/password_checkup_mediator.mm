// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/password_checkup_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::InsecurePasswordCounts;

namespace {

// Returns true if a password check error occured.
bool DidPasswordCheckupFail(PasswordCheckState currentState) {
  switch (currentState) {
    case PasswordCheckState::kRunning:
    case PasswordCheckState::kNoPasswords:
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle:
      return false;
    case PasswordCheckState::kOffline:
    case PasswordCheckState::kSignedOut:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther:
      return true;
  }
}

}  // namespace

@interface PasswordCheckupMediator () <PasswordCheckObserver> {
  // The service responsible for password check feature.
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;

  // A helper object for passing data about changes in password check status
  // and changes to compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  // Current password counts associated with the different insecure types.
  InsecurePasswordCounts _currentInsecurePasswordCounts;

  // The string containing the timestamp of the last completed check.
  NSString* _formattedElapsedTimeSinceLastCheck;

  // Current number of affiliated groups for which the user has saved passwords.
  int _currentAffiliatedGroupCount;
}

// Current state of password check.
@property(nonatomic, assign) PasswordCheckState currentState;

@end

@implementation PasswordCheckupMediator

- (instancetype)initWithPasswordCheckManager:
    (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager {
  self = [super init];
  if (self) {
    _passwordCheckManager = passwordCheckManager;
    _passwordCheckObserver = std::make_unique<PasswordCheckObserverBridge>(
        self, _passwordCheckManager.get());
    _currentState = _passwordCheckManager->GetPasswordCheckState();
  }
  return self;
}

- (void)setConsumer:(id<PasswordCheckupConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  [self updateConsumer];
}

- (void)disconnect {
  _passwordCheckObserver.reset();
  _passwordCheckManager.reset();
}

#pragma mark - PasswordCheckupViewControllerDelegate

- (void)startPasswordCheck {
  _passwordCheckManager->StartPasswordCheck();
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  self.currentState = state;
}

- (void)insecureCredentialsDidChange {
  // Insecure password changes have no effect on UI while check is running.
  if (_passwordCheckManager->GetPasswordCheckState() ==
      PasswordCheckState::kRunning) {
    return;
  }

  [self updateConsumer];
}

#pragma mark - Setters

- (void)setCurrentState:(PasswordCheckState)state {
  if (state == _currentState) {
    return;
  }
  _currentState = state;
  [self updateConsumer];
}

#pragma mark - Private Methods

// Updates the `_consumer` PasswordCheckupHomepageState, the number of
// affiliated groups and the the insecure password counts.
- (void)updateConsumer {
  DCHECK(self.consumer);

  PasswordCheckupHomepageState passwordCheckupHomepageState =
      [self computePasswordCheckupHomepageState];

  if (!DidPasswordCheckupFail(_currentState)) {
    std::vector<password_manager::CredentialUIEntry> insecureCredentials =
        _passwordCheckManager->GetInsecureCredentials();
    _currentInsecurePasswordCounts =
        password_manager::CountInsecurePasswordsPerInsecureType(
            insecureCredentials);

    _currentAffiliatedGroupCount =
        _passwordCheckManager->GetSavedPasswordsPresenter()
            ->GetAffiliatedGroups()
            .size();

    _formattedElapsedTimeSinceLastCheck =
        [self formattedElapsedTimeSinceLastCheck];
  }

  [self.consumer
         setPasswordCheckupHomepageState:passwordCheckupHomepageState
                  insecurePasswordCounts:_currentInsecurePasswordCounts
      formattedElapsedTimeSinceLastCheck:_formattedElapsedTimeSinceLastCheck];
  [self.consumer setAffiliatedGroupCount:_currentAffiliatedGroupCount];
  if (DidPasswordCheckupFail(_currentState)) {
    [self.consumer showErrorDialogWithMessage:[self computeErrorDialogMessage]];
  }
}

// Returns PasswordCheckupHomepageState based on the current PasswordCheckState.
- (PasswordCheckupHomepageState)computePasswordCheckupHomepageState {
  switch (_currentState) {
    case PasswordCheckState::kRunning:
      return PasswordCheckupHomepageStateRunning;
    case PasswordCheckState::kNoPasswords:
      return PasswordCheckupHomepageStateDisabled;
    case PasswordCheckState::kSignedOut:
    case PasswordCheckState::kOffline:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther:
      return PasswordCheckupHomepageStateDone;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle:
      return PasswordCheckupHomepageStateDone;
  }
}

// Returns the string containing the timestamp of the last password check.
- (NSString*)formattedElapsedTimeSinceLastCheck {
  absl::optional<base::Time> lastCompletedCheck =
      _passwordCheckManager->GetLastPasswordCheckTime();
  return password_manager::FormatElapsedTimeSinceLastCheck(
      lastCompletedCheck, /*use_title_case=*/true);
}

// Computes the error message to display in the error dialog.
- (NSString*)computeErrorDialogMessage {
  switch (_currentState) {
    case PasswordCheckState::kRunning:
    case PasswordCheckState::kNoPasswords:
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle:
      return nil;
    case PasswordCheckState::kOffline:
      return l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_OFFLINE);
    case PasswordCheckState::kSignedOut:
    case PasswordCheckState::kQuotaLimit:
    case PasswordCheckState::kOther:
      return l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_OTHER);
  }
}

@end
