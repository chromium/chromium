// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_suggestion_bottom_sheet_mediator.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate_factory.h"
#import "components/webauthn/ios/passkey_suggestion_utils.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base+Subclassing.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface PasskeySuggestionBottomSheetMediator ()

// Delegate used to fetch and select passkey suggestions.
@property(nonatomic, assign) raw_ptr<webauthn::IOSWebAuthnCredentialsDelegate>
    webAuthnCredentialsDelegate;

@end

@implementation PasskeySuggestionBottomSheetMediator

- (instancetype)
    initWithWebStateList:(WebStateList*)webStateList
             requestInfo:(webauthn::IOSPasskeyClient::RequestInfo)requestInfo
            reauthModule:(id<ReauthenticationProtocol>)reauthModule {
  std::optional<autofill::RemoteFrameToken> remoteFrameToken =
      requestInfo.remote_frame_token;

  self = [super initWithWebStateList:webStateList
                        reauthModule:reauthModule
                         requestInfo:std::move(requestInfo)];
  if (self) {
    if (remoteFrameToken.has_value()) {
      __weak __typeof(self) weakSelf = self;
      auto callback = base::BindOnce(
          [](PasskeySuggestionBottomSheetMediator* mediator,
             webauthn::IOSWebAuthnCredentialsDelegate* delegate) {
            mediator.webAuthnCredentialsDelegate = delegate;
          },
          weakSelf);

      webauthn::IOSWebAuthnCredentialsDelegateFactory::GetFactory(
          webStateList->GetActiveWebState())
          ->GetDelegateForRemoteFrameToken(*remoteFrameToken,
                                           std::move(callback));
    }
  }

  return self;
}

- (void)setWebAuthnCredentialsDelegate:
    (raw_ptr<webauthn::IOSWebAuthnCredentialsDelegate>)delegate {
  _webAuthnCredentialsDelegate = delegate;
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

#pragma mark - CredentialSuggestionBottomSheetMediatorBase

- (void)setConsumer:(id<CredentialSuggestionBottomSheetConsumer>)consumer {
  [super setConsumer:consumer];

  // The bottom sheet isn't presented when there are no suggestions to show, so
  // there's no need to update the consumer.
  if (![self hasSuggestions]) {
    return;
  }

  [self.consumer
      setPrimaryActionString:l10n_util::GetNSString(
                                 IDS_IOS_CREDENTIAL_BOTTOM_SHEET_CONTINUE)
       secondaryActionString:l10n_util::GetNSString(
                                 IDS_IOS_CREDENTIAL_BOTTOM_SHEET_MORE_PASSKEYS)
        secondaryActionImage:DefaultSymbolWithPointSize(
                                 kPersonBadgeKeyFillSymbol,
                                 kSymbolActionPointSize)];
}

- (void)disconnect {
  [super disconnect];

  _webAuthnCredentialsDelegate = nullptr;
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                 completion:(ProceduralBlock)completion {
  CHECK_EQ(suggestion.type, autofill::SuggestionType::kWebauthnCredential);
  [super didSelectSuggestion:suggestion atIndex:index completion:completion];
}

#pragma mark - Subclassing

// Perform suggestion selection
- (void)selectSuggestion:(FormSuggestion*)suggestion
                 atIndex:(NSInteger)index
              completion:(ProceduralBlock)completion {
  // `webAuthnCredentialsDelegate` can be null if the frame it was created for
  // was destroyed or navigated away.
  if (!_webAuthnCredentialsDelegate) {
    completion();
    return;
  }

  _webAuthnCredentialsDelegate->SelectPasskey(
      webauthn::GetPasskeySuggestionEncodedCredentialId(suggestion),
      base::BindOnce(completion));
}

@end
