// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator_delegate.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/passkey_creation_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PasskeyCreationBottomSheetMediator () <WebStateListObserving>
@end

@implementation PasskeyCreationBottomSheetMediator {
  // Delegate that controls the presentation of the bottom sheet.
  __weak id<PasskeyCreationBottomSheetMediatorDelegate> _mediatorDelegate;

  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;

  // Bridge for observing WebStateList events.
  std::optional<WebStateListObserverBridge> _webStateListObserver;
  std::optional<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  // ID of the passkey request.
  std::string _requestID;

  // Email associated with the account the passkey will get saved in.
  NSString* _accountForSaving;

  // Module containing the reauthentication mechanism.
  __weak id<ReauthenticationProtocol> _reauthModule;

  // URL of the current page the bottom sheet is being displayed on.
  GURL _URL;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                           requestID:(std::string)requestID
                    accountForSaving:(NSString*)accountForSaving
                        reauthModule:(id<ReauthenticationProtocol>)reauthModule
                            delegate:
                                (id<PasskeyCreationBottomSheetMediatorDelegate>)
                                    mediatorDelegate {
  CHECK(!requestID.empty());
  self = [super init];
  if (self) {
    _webStateList = webStateList;

    // Create and register the observers.
    _webStateListObserver.emplace(self);
    _webStateListObservation.emplace(&(*_webStateListObserver));
    _webStateListObservation->Observe(_webStateList);

    _requestID = requestID;
    _accountForSaving = accountForSaving;
    _reauthModule = reauthModule;
    _URL = webStateList->GetActiveWebState()->GetLastCommittedURL();
    _mediatorDelegate = mediatorDelegate;
  }
  return self;
}

- (void)disconnect {
  _webStateListObservation.reset();
  _webStateListObserver.reset();
  _webStateList = nullptr;
}

- (void)createPasskey {
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper) {
    return;
  }

  std::optional<bool> shouldPerformUserVerification =
      passkeyTabHelper->ShouldPerformUserVerification(
          _requestID, [_reauthModule canAttemptReauthWithBiometrics]);

  if (!shouldPerformUserVerification.has_value()) {
    // TODO(crbug.com/479249845): This should not happen. The correct behavior
    // is to, optionally, report an error to the user, and cancel the passkey
    // creation. We will discuss to see if this is the best approach.
    [self cancelPasskeyCreation];
    [_mediatorDelegate dismissPasskeyCreation];
    return;
  }

  if (!*shouldPerformUserVerification) {
    [self performPasskeyCreation];
    return;
  }

  if ([_reauthModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    [_reauthModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_START_REAUTH_REASON)
                    canReusePreviousAuth:YES
                                 handler:^(ReauthenticationResult result) {
                                   [weakSelf handleReauthResult:result];
                                 }];
    return;
  }

  // TODO(crbug.com/479249845): The code below is the correct behavior.
  // We need to review it with broader teams to confirm that this is the best
  // approach. When reauthentication cannot be attempted, we
  // could fail the request or show an appropriate error to the user. However,
  // there might be other means for the device to authenticate the user (other
  // credential providers in the system). So, deferring to the renderer is a
  // reasonable fallback.
  [self deferPasskeyCreationToRenderer];
  [_mediatorDelegate dismissPasskeyCreation];
}

- (void)handleReauthResult:(ReauthenticationResult)result {
  if (result == ReauthenticationResult::kSuccess) {
    [self performPasskeyCreation];
  } else {
    // TODO(crbug.com/479249845): The correct behavior when reauthentication
    // fails (e.g., was canceled) should be to fail the request.
    // We could allow a certain number of retries. However, given that a user
    // can trigger passkey creation from the web later, dismissing passkey
    // creation isn't an irreversible action.
    [self cancelPasskeyCreation];
    [_mediatorDelegate dismissPasskeyCreation];
  }
}

- (void)performPasskeyCreation {
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper) {
    [_mediatorDelegate dismissPasskeyCreation];
    return;
  }
  passkeyTabHelper->StartPasskeyCreation(_requestID);
  [_mediatorDelegate dismissPasskeyCreation];
}

- (void)deferPasskeyCreationToRenderer {
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper) {
    return;
  }

  passkeyTabHelper->DeferPendingRequestToRenderer(_requestID);
}

- (void)cancelPasskeyCreation {
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper) {
    return;
  }

  passkeyTabHelper->RejectPendingRequest(_requestID);
}

#pragma mark - Accessors

- (void)setConsumer:(id<PasskeyCreationBottomSheetConsumer>)consumer {
  _consumer = consumer;
  [_consumer setUsername:[self username] email:[self email] url:_URL];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  CHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self onWebStateChanged];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  CHECK_EQ(webStateList, _webStateList);
  [self onWebStateChanged];
}

#pragma mark - Private

// Returns the username for the passkey request.
- (NSString*)username {
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper) {
    return nil;
  }

  const std::string& username =
      passkeyTabHelper->UsernameForRequest(_requestID);
  if (username.empty()) {
    return nil;
  }

  return base::SysUTF8ToNSString(username);
}

// Returns the email for the passkey request.
- (NSString*)email {
  return _accountForSaving;
}

// Closes the currently open bottom sheet when the web state changes.
- (void)onWebStateChanged {
  // Disconnect so anything that relies on the webstate behind the mediator can
  // avoid using the mediator's objects once the webstate is destroyed.
  [self disconnect];

  // As there is no more context for showing the bottom sheet, end the
  // presentation.
  [_mediatorDelegate endPresentation];
}

// Returns the webauthn::PasskeyTabHelper for the active webstate or nil if it
// can't be retrieved.
- (webauthn::PasskeyTabHelper*)passkeyTabHelper {
  if (!_webStateList) {
    return nil;
  }

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return nil;
  }

  return webauthn::PasskeyTabHelper::FromWebState(activeWebState);
}

@end
