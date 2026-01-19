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
#import "ios/web/public/web_state.h"

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

  // URL of the current page the bottom sheet is being displayed on.
  GURL _URL;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                           requestID:(std::string)requestID
                    accountForSaving:(NSString*)accountForSaving
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

  passkeyTabHelper->StartPasskeyCreation(_requestID);
}

- (void)deferPasskeyCreationToRenderer {
  webauthn::PasskeyTabHelper* passkeyTabHelper = [self passkeyTabHelper];
  if (!passkeyTabHelper) {
    return;
  }

  passkeyTabHelper->DeferPendingRequestToRenderer(_requestID);
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
