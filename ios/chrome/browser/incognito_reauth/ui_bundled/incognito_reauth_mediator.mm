// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_mediator.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_consumer.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_lock_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface IncognitoReauthMediator () <IncognitoStateObserver>

// Incognito state object tracking the authentication status.
@property(nonatomic, weak) IncognitoState* incognitoState;

@end

@implementation IncognitoReauthMediator

- (instancetype)initWithIncognitoState:(IncognitoState*)incognitoState {
  self = [super init];
  if (self) {
    _incognitoState = incognitoState;
    [incognitoState addObserver:self];
  }
  return self;
}

- (void)setConsumer:(id<IncognitoReauthConsumer>)consumer {
  _consumer = consumer;
  if (IsIOSSoftLockEnabled()) {
    [self notifyConsumer:_incognitoState.lockState];
  } else {
    [self.consumer
        setItemsRequireAuthentication:_incognitoState.authenticationRequired];
  }
}

- (void)dealloc {
  [_incognitoState removeObserver:self];
}

#pragma mark - IncognitoStateObserver

- (void)didUpdateAuthenticationRequirementForState:
    (IncognitoState*)incognitoState {
  CHECK(!IsIOSSoftLockEnabled());
  [self.consumer
      setItemsRequireAuthentication:incognitoState.authenticationRequired];
}

- (void)didUpdateIncognitoLockStateForState:(IncognitoState*)incognitoState {
  CHECK(IsIOSSoftLockEnabled());
  [self notifyConsumer:incognitoState.lockState];
}

#pragma mark - Private

// Notify the consumer of the new authentication requirements, primary button
// text and primary button accessibility label, when the incognito lock state
// changes.
- (void)notifyConsumer:(IncognitoLockState)incognitoLockState {
  BOOL isAuthenticationRequired =
      incognitoLockState != IncognitoLockState::kNone;
  NSString* primaryButtonText =
      [self primaryButtonTextForState:incognitoLockState];
  NSString* accessibilityLabel =
      [self primaryButtonAccessibilityLabelForState:incognitoLockState];

  [self.consumer setItemsRequireAuthentication:isAuthenticationRequired
                         withPrimaryButtonText:primaryButtonText
                            accessibilityLabel:accessibilityLabel];
}

// Returns the text that should be displayed by the lock screen primary button.
// Returns nil if no lock screen should be displayed.
- (NSString*)primaryButtonTextForState:(IncognitoLockState)incognitoLockState {
  switch (incognitoLockState) {
    case IncognitoLockState::kSoftLock:
      return l10n_util::GetNSString(
          IDS_IOS_INCOGNITO_REAUTH_CONTINUE_IN_INCOGNITO);
    case IncognitoLockState::kReauth:
      return l10n_util::GetNSStringF(
          IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
          base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
    case IncognitoLockState::kNone:
      return nil;
  }
}

// Returns the accessibility label that should be attached to the lock screen
// primary button. Returns nil if no lock screen should be displayed.
- (NSString*)primaryButtonAccessibilityLabelForState:
    (IncognitoLockState)incognitoLockState {
  switch (incognitoLockState) {
    case IncognitoLockState::kSoftLock:
      return l10n_util::GetNSString(
          IDS_IOS_INCOGNITO_REAUTH_CONTINUE_IN_INCOGNITO);
    case IncognitoLockState::kReauth:
      return l10n_util::GetNSStringF(
          IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
          base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
    case IncognitoLockState::kNone:
      return nil;
  }
}

@end
