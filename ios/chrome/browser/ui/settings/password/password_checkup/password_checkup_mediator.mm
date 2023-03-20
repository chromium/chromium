// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::InsecurePasswordCounts;

@interface PasswordCheckupMediator () <PasswordCheckObserver> {
  // The service responsible for password check feature.
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;

  // A helper object for passing data about changes in password check status
  // and changes to compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;
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

  std::vector<password_manager::CredentialUIEntry> insecureCredentials =
      _passwordCheckManager->GetInsecureCredentials();
  password_manager::InsecurePasswordCounts insecurePasswordCounts =
      password_manager::CountInsecurePasswordsPerInsecureType(
          insecureCredentials);

  PasswordCheckupHomepageState passwordCheckupHomepageState =
      [self computePasswordCheckupHomepageState];
  int affiliatedGroupCount = _passwordCheckManager->GetSavedPasswordsPresenter()
                                 ->GetAffiliatedGroups()
                                 .size();

  [self.consumer setPasswordCheckupHomepageState:passwordCheckupHomepageState
                          insecurePasswordCounts:insecurePasswordCounts
              formattedElapsedTimeSinceLastCheck:
                  [self formattedElapsedTimeSinceLastCheck]];
  [self.consumer setAffiliatedGroupCount:affiliatedGroupCount];
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
      return PasswordCheckupHomepageStateError;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle:
      return PasswordCheckupHomepageStateDone;
  }
}

- (NSString*)formattedElapsedTimeSinceLastCheck {
  absl::optional<base::Time> lastCompletedCheck =
      _passwordCheckManager->GetLastPasswordCheckTime();
  return password_manager::FormatElapsedTimeSinceLastCheck(lastCompletedCheck);
}

@end
