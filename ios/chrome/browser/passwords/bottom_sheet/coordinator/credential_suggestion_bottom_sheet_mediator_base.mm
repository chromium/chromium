// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base.h"

#import "base/notreached.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/webauthn/ios/ios_passkey_client.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base+Subclassing.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/password_suggestion_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_presenter.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "url/origin.h"

@interface CredentialSuggestionBottomSheetMediatorBase () <
    WebStateListObserving>

// Origin to fetch credentials for.
@property(nonatomic, assign) GURL URL;

// Domain of the URL to fetch credentials for.
@property(nonatomic, strong) NSString* domain;

// List of suggestions to be shown in the bottom sheet.
@property(nonatomic, strong) NSArray<FormSuggestion*>* suggestions;

// The WebStateList observed by this mediator.
@property(nonatomic, readonly) WebStateList* webStateList;

@end

@implementation CredentialSuggestionBottomSheetMediatorBase {
  // Bridge for observing WebStateList events.
  std::optional<WebStateListObserverBridge> _webStateListObserver;
  std::optional<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  // Module containing the reauthentication mechanism.
  id<ReauthenticationProtocol> _reauthenticationModule;

  // Information about the pending passkey request.
  std::optional<webauthn::IOSPasskeyClient::RequestInfo> _requestInfo;
}

- (instancetype)
    initWithWebStateList:(WebStateList*)webStateList
            reauthModule:(id<ReauthenticationProtocol>)reauthModule
             requestInfo:
                 (std::optional<webauthn::IOSPasskeyClient::RequestInfo>)
                     requestInfo {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _reauthenticationModule = reauthModule;
    _requestInfo = std::move(requestInfo);

    // Create and register the observers.
    _webStateListObserver.emplace(self);
    _webStateListObservation.emplace(&(*_webStateListObserver));
    _webStateListObservation->Observe(_webStateList);

    _URL = _webStateList->GetActiveWebState()->GetLastCommittedURL();

    _domain = @"";
    if (!_URL.is_empty()) {
      url::Origin origin = url::Origin::Create(_URL);
      _domain =
          base::SysUTF8ToNSString(password_manager::GetShownOrigin(origin));
    }
  }
  return self;
}

- (void)setConsumer:(id<CredentialSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;

  // The bottom sheet isn't presented when there are no suggestions to show, so
  // there's no need to update the consumer.
  if (![self hasSuggestions]) {
    return;
  }

  [_consumer setSuggestions:self.suggestions andDomain:self.domain];
}

- (void)disconnect {
  _webStateListObservation.reset();
  _webStateListObserver.reset();
  _webStateList = nullptr;
  _reauthenticationModule = nil;
}

- (BOOL)hasSuggestions {
  return [self.suggestions count] > 0;
}

- (void)didSelectSuggestion:(FormSuggestion*)formSuggestion
                    atIndex:(NSInteger)index
                 completion:(ProceduralBlock)completion {
  if (!formSuggestion.requiresReauth) {
    [self selectSuggestion:formSuggestion atIndex:index completion:completion];
    return;
  }
  if ([_reauthenticationModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    auto completionHandler = ^(ReauthenticationResult result) {
      [weakSelf selectSuggestion:formSuggestion
                         atIndex:index
          reauthenticationResult:result
                      completion:completion];
    };

    NSString* reason = l10n_util::GetNSString(IDS_IOS_AUTOFILL_REAUTH_REASON);
    [_reauthenticationModule
        attemptReauthWithLocalizedReason:reason
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    [self selectSuggestion:formSuggestion atIndex:index completion:completion];
  }
}

- (void)selectSuggestion:(FormSuggestion*)suggestion
                 atIndex:(NSInteger)index
              completion:(ProceduralBlock)completion {
  // Must be implemented by subclasses.
  NOTREACHED();
}

- (void)selectSuggestion:(FormSuggestion*)suggestion
                   atIndex:(NSInteger)index
    reauthenticationResult:(ReauthenticationResult)result
                completion:(ProceduralBlock)completion {
  if (result != ReauthenticationResult::kFailure) {
    [self selectSuggestion:suggestion atIndex:index completion:completion];
  } else {
    [self disconnect];
    if (completion) {
      completion();
    }
  }
}

- (void)logExitReason:(PasswordSuggestionBottomSheetExitReason)exitReason {
}

- (void)onDismissWithoutAnyCredentialAction {
}

- (BOOL)hasPendingRequest:
    (const webauthn::IOSPasskeyClient::RequestInfo&)requestInfo {
  return _requestInfo.has_value() && *_requestInfo == requestInfo;
}

#pragma mark - CredentialSuggestionBottomSheetDelegate

- (void)disableBottomSheet {
}

- (void)loadFaviconWithBlockHandler:
    (FaviconLoader::FaviconAttributesCompletionBlock)faviconLoadedBlock {
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self onWebStateChanged];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(webStateList, _webStateList);
  [self onWebStateChanged];
}

#pragma mark - Private

// Closes the current bottom sheet when the web state changes.
- (void)onWebStateChanged {
  // Disconnect so anything that relies on the webstate behind the mediator can
  // avoid using the mediator's objects once the webstate is destroyed.
  [self disconnect];

  // As there is no more context for showing the bottom sheet, end the
  // presentation.
  [self.presenter endPresentation];
}

@end
