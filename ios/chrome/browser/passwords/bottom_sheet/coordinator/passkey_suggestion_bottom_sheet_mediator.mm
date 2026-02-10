// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate_factory.h"
#import "components/webauthn/ios/passkey_suggestion_utils.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base+Subclassing.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@implementation PasskeySuggestionBottomSheetMediator {
  // The WebStateList observed by this mediator and the observer bridge.
  // TODO(crbug.com/464290670): Make this mediator observe the WebStateList via
  // CredentialSuggestionBottomSheetMediatorBase.
  raw_ptr<WebStateList> _webStateList;

  // Information of the passkey request which triggered the bottom sheet.
  std::unique_ptr<webauthn::IOSPasskeyClient::RequestInfo> _requestInfo;

  // Delegate used to fetch and select passkey suggestions.
  raw_ptr<webauthn::IOSWebAuthnCredentialsDelegate>
      _webAuthnCredentialsDelegate;
}

@synthesize consumer = _consumer;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                         requestInfo:(webauthn::IOSPasskeyClient::RequestInfo)
                                         requestInfo {
  if ((self = [super init])) {
    _webStateList = webStateList;
    _requestInfo = std::make_unique<webauthn::IOSPasskeyClient::RequestInfo>(
        std::move(requestInfo));

    _webAuthnCredentialsDelegate =
        webauthn::IOSWebAuthnCredentialsDelegateFactory::GetFactory(
            _webStateList->GetActiveWebState())
            ->GetDelegateForFrame(_requestInfo->frame_id);
    if (_webAuthnCredentialsDelegate) {
      base::expected<const std::vector<password_manager::PasskeyCredential>*,
                     password_manager::WebAuthnCredentialsDelegate::
                         PasskeysUnavailableReason>
          passkeys = _webAuthnCredentialsDelegate->GetPasskeys();
      if (passkeys.has_value()) {
        self.suggestions =
            webauthn::FormSuggestionsFromPasskeyCredentials(**passkeys);
      }
    }
  }

  return self;
}

- (void)setConsumer:(id<CredentialSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;
  if ([self hasSuggestions]) {
    // TODO(crbug.com/464290670): Pass actual domain.
    [consumer setSuggestions:self.suggestions andDomain:@""];
  }
}

#pragma mark - CredentialSuggestionBottomSheetMediatorBase

- (void)disconnect {
  _webStateList = nullptr;
  _requestInfo.reset();
  _webAuthnCredentialsDelegate = nullptr;
}

@end
