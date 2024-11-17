// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_mediator.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_consumer.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface IncognitoReauthMediator () <IncognitoReauthObserver>

// Agent tracking the authentication status.
@property(nonatomic, weak) IncognitoReauthSceneAgent* reauthAgent;

@end

@implementation IncognitoReauthMediator

- (instancetype)initWithReauthAgent:(IncognitoReauthSceneAgent*)reauthAgent {
  self = [super init];
  if (self) {
    _reauthAgent = reauthAgent;
    [reauthAgent addObserver:self];
  }
  return self;
}

- (void)setConsumer:(id<IncognitoReauthConsumer>)consumer {
  _consumer = consumer;
  if (IsIOSSoftLockEnabled()) {
    [self notifyConsumer:_reauthAgent.incognitoLockState];
  } else {
    [_consumer
        setItemsRequireAuthentication:_reauthAgent.authenticationRequired];
  }
}

- (void)dealloc {
  [_reauthAgent removeObserver:self];
}

#pragma mark - IncognitoReauthObserver

- (void)reauthAgent:(IncognitoReauthSceneAgent*)agent
    didUpdateAuthenticationRequirement:(BOOL)isRequired {
  CHECK(!IsIOSSoftLockEnabled());
  [self.consumer setItemsRequireAuthentication:isRequired];
}

- (void)reauthAgent:(IncognitoReauthSceneAgent*)agent
    didUpdateIncognitoLockState:(IncognitoLockState)incognitoLockState {
  CHECK(IsIOSSoftLockEnabled());
  [self notifyConsumer:incognitoLockState];
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
- (NSString*)primaryButtonTextForState:(IncognitoLockState)incogitoLockState {
  switch (incogitoLockState) {
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
