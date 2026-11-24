// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_exit_reason.h"
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

  // The WebState associated with the pending passkey request. Stored as a weak
  // pointer so cancellation still targets the originating WebState even after
  // the active WebState changes.
  base::WeakPtr<web::WebState> _webState;

  // Bridge for observing WebStateList events.
  std::optional<WebStateListObserverBridge> _webStateListObserver;
  std::optional<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  // Information about the pending passkey request.
  std::optional<webauthn::IOSPasskeyClient::RequestInfo> _requestInfo;

  // Email associated with the account the passkey will get saved in.
  NSString* _accountForSaving;

  // Module containing the reauthentication mechanism.
  id<ReauthenticationProtocol> _reauthModule;

  // Whether a reauthentication request is currently in flight.
  BOOL _reauthInProgress;

  // Ensures at most one exit reason is logged per bottom sheet instance.
  BOOL _exitReasonLogged;
}

- (instancetype)
    initWithWebStateList:(WebStateList*)webStateList
             requestInfo:(webauthn::IOSPasskeyClient::RequestInfo)requestInfo
        accountForSaving:(NSString*)accountForSaving
            reauthModule:(id<ReauthenticationProtocol>)reauthModule
                delegate:(id<PasskeyCreationBottomSheetMediatorDelegate>)
                             mediatorDelegate {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    web::WebState* activeWebState = _webStateList->GetActiveWebState();
    if (activeWebState) {
      _webState = activeWebState->GetWeakPtr();
    }

    // Create and register the observers.
    _webStateListObserver.emplace(self);
    _webStateListObservation.emplace(&(*_webStateListObserver));
    _webStateListObservation->Observe(_webStateList);

    _requestInfo = std::move(requestInfo);
    _accountForSaving = accountForSaving;
    _reauthModule = reauthModule;
    _mediatorDelegate = mediatorDelegate;
  }
  return self;
}

- (void)disconnect {
  _webStateListObservation.reset();
  _webStateListObserver.reset();
  _webStateList = nullptr;
  _webState.reset();
  _requestInfo.reset();
  _reauthInProgress = NO;
}

- (void)createPasskey {
  if (_reauthInProgress) {
    return;
  }

  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper || !_requestInfo.has_value()) {
    _requestInfo.reset();
    [self logExitReason:PasskeyCreationBottomSheetExitReason::kInvalidRequest];
    [_mediatorDelegate dismissPasskeyCreation];
    return;
  }

  std::optional<bool> shouldPerformUserVerification =
      passkeyTabHelper->ShouldPerformUserVerification(_requestInfo->request_id);

  if (!shouldPerformUserVerification.has_value()) {
    // TODO(crbug.com/479249845): This should not happen. The correct behavior
    // is to, optionally, report an error to the user, and cancel the passkey
    // creation. We will discuss to see if this is the best approach.
    _requestInfo.reset();
    [self logExitReason:PasskeyCreationBottomSheetExitReason::kInvalidRequest];
    [_mediatorDelegate dismissPasskeyCreation];
    return;
  }

  bool mustPerformUserVerification = *shouldPerformUserVerification;
  if (!mustPerformUserVerification) {
    [self performPasskeyCreationWithUserVerification:NO];
    return;
  }

  if ([_reauthModule canAttemptReauth]) {
    _reauthInProgress = YES;
    __weak __typeof(self) weakSelf = self;
    [_reauthModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_START_REAUTH_REASON)
                    canReusePreviousAuth:!mustPerformUserVerification
                                 handler:^(ReauthenticationResult result) {
                                   [weakSelf handleReauthResult:result];
                                 }];
    return;
  }

  // Defer to the OS to allow authentication methods that don't require a
  // passcode. (e.g., security keys, using a QR code).
  [self logExitReason:PasskeyCreationBottomSheetExitReason::kReauthNotPossible];
  [self deferPasskeyCreationToRenderer];
  [_mediatorDelegate dismissPasskeyCreation];
}

- (void)handleReauthResult:(ReauthenticationResult)result {
  _reauthInProgress = NO;
  if (!_requestInfo.has_value()) {
    return;
  }

  if (result != ReauthenticationResult::kFailure) {
    [self performPasskeyCreationWithUserVerification:
              result == ReauthenticationResult::kSuccess];
  } else {
    // TODO(crbug.com/479249845): The correct behavior when reauthentication
    // fails (e.g., was canceled) should be to fail the request.
    // We could allow a certain number of retries. However, given that a user
    // can trigger passkey creation from the web later, dismissing passkey
    // creation isn't an irreversible action.
    [self logExitReason:PasskeyCreationBottomSheetExitReason::kReauthFailed];
    [self cancelPasskeyCreation];
    [_mediatorDelegate dismissPasskeyCreation];
  }
}

- (void)performPasskeyCreationWithUserVerification:(BOOL)didCompleteUV {
  std::optional<std::string> requestId = [self takePendingRequestId];
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper || !requestId.has_value()) {
    [self logExitReason:PasskeyCreationBottomSheetExitReason::kInvalidRequest];
    [_mediatorDelegate dismissPasskeyCreation];
    return;
  }
  [self logExitReason:PasskeyCreationBottomSheetExitReason::kCreatePasskey];
  passkeyTabHelper->StartPasskeyCreation(std::move(*requestId), didCompleteUV);
  [_mediatorDelegate dismissPasskeyCreation];
}

- (void)deferPasskeyCreationToRenderer {
  std::optional<std::string> requestId = [self takePendingRequestId];
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper || !requestId.has_value()) {
    return;
  }

  passkeyTabHelper->DeferPendingRequestToRenderer(*requestId);
}

- (void)cancelPasskeyCreation {
  std::optional<std::string> requestId = [self takePendingRequestId];
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper || !requestId.has_value()) {
    return;
  }

  passkeyTabHelper->RejectPendingRequest(*requestId);
}

- (void)logExitReason:(PasskeyCreationBottomSheetExitReason)exitReason {
  if (_exitReasonLogged) {
    return;
  }
  _exitReasonLogged = YES;
  base::UmaHistogramEnumeration("IOS.PasskeyCreationBottomSheet.ExitReason",
                                exitReason);
}

- (BOOL)hasPendingRequest:
    (const webauthn::IOSPasskeyClient::RequestInfo&)requestInfo {
  return _requestInfo.has_value() && *_requestInfo == requestInfo;
}

#pragma mark - Accessors

- (void)setConsumer:(id<PasskeyCreationBottomSheetConsumer>)consumer {
  if (!consumer) {
    _consumer = nil;
    return;
  }

  NSString* rpId = [self rpId];
  if (!rpId) {
    // The RP ID should not be empty; log the exit reason and leave `_consumer`
    // unset so the coordinator dismisses without presenting.
    [self logExitReason:PasskeyCreationBottomSheetExitReason::kInvalidRequest];
    return;
  }

  _consumer = consumer;
  [_consumer setUsername:[self username] email:[self email] rpId:rpId];
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

// Returns the relying party identifier for the passkey request.
- (NSString*)rpId {
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper || !_requestInfo.has_value()) {
    return nil;
  }

  std::string rpId =
      passkeyTabHelper->RelyingPartyIdForRequest(_requestInfo->request_id);
  if (rpId.empty()) {
    return nil;
  }

  return base::SysUTF8ToNSString(rpId);
}

// Returns the username for the passkey request.
- (NSString*)username {
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper || !_requestInfo.has_value()) {
    return nil;
  }

  const std::string& username =
      passkeyTabHelper->UsernameForRequest(_requestInfo->request_id);
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
  [self
      logExitReason:PasskeyCreationBottomSheetExitReason::kDismissedExternally];

  // Reject any pending passkey creation request on the originating WebState
  // before disconnecting so the page's promise does not hang until timeout.
  [self cancelPasskeyCreation];

  // Disconnect so anything that relies on the webstate behind the mediator can
  // avoid using the mediator's objects once the webstate is destroyed.
  [self disconnect];

  // As there is no more context for showing the bottom sheet, end the
  // presentation.
  [_mediatorDelegate endPresentation];
}

// Consumes and returns the pending request ID, or `std::nullopt` if the
// request has already been handled or cleared.
- (std::optional<std::string>)takePendingRequestId {
  if (!_requestInfo.has_value()) {
    return std::nullopt;
  }

  std::string requestId = std::move(_requestInfo->request_id);
  _requestInfo.reset();
  return requestId;
}

// Returns the `webauthn::PasskeyTabHelper` for the originating webstate or
// nullptr if it can't be retrieved.
- (webauthn::PasskeyTabHelper*)passkeyTabHelper {
  if (!_webState) {
    return nullptr;
  }

  return webauthn::PasskeyTabHelper::FromWebState(_webState.get());
}

@end
