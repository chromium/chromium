// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/debugger/omnibox_debugger_mediator.h"

#import <optional>
#import <string>

#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/remote_suggestions_service.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/omnibox/debugger/omnibox_autocomplete_event.h"
#import "ios/chrome/browser/omnibox/debugger/omnibox_debugger_consumer.h"
#import "ios/chrome/browser/omnibox/debugger/omnibox_remote_suggestion_event.h"
#import "ios/chrome/browser/omnibox/debugger/remote_suggestions_service_delegate_bridge.h"
#import "ios/chrome/browser/omnibox/debugger/remote_suggestions_service_observer_bridge.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_controller_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "services/network/public/cpp/resource_request.h"

@interface OmniboxDebuggerMediator () <RemoteSuggestionsServiceDelegate>

@end

@implementation OmniboxDebuggerMediator {
  // Autocomolete controller.
  raw_ptr<AutocompleteController> _autocompleteController;
  // Remote suggestions service.
  raw_ptr<RemoteSuggestionsService> _remoteSuggestionsService;
  // Autocomplete controller observer bridge.
  std::unique_ptr<AutocompleteControllerObserverBridge>
      _autocompleteObserverBridge;
  // Remote suggestions service observer bridge.
  std::unique_ptr<RemoteSuggestionsServiceObserverBridge>
      _remoteSuggestionsServiceObserverBridge;
  // Remote suggestions service observer bridge.
  std::unique_ptr<RemoteSuggestionsServiceDelegateBridge>
      _remoteSuggestionsServiceDelegateBridge;
  // Hardcoded suggest response.
  std::string _hardcodedSuggestResponse;
}

- (instancetype)initWithAutocompleteController:
                    (AutocompleteController*)autocompleteController
                      remoteSuggestionsService:
                          (RemoteSuggestionsService*)remoteSuggestionsService {
  self = [super init];

  if (self) {
    _autocompleteController = autocompleteController;
    _remoteSuggestionsService = remoteSuggestionsService;
  }

  return self;
}

- (void)disconnect {
  if (_remoteSuggestionsServiceObserverBridge) {
    _remoteSuggestionsService->RemoveObserver(
        _remoteSuggestionsServiceObserverBridge.get());
    _remoteSuggestionsServiceObserverBridge.reset();
  }

  if (_remoteSuggestionsServiceDelegateBridge) {
    _remoteSuggestionsService->SetDelegate(nullptr);
    _remoteSuggestionsServiceDelegateBridge.reset();
  }

  if (_autocompleteObserverBridge) {
    _autocompleteController->RemoveObserver(_autocompleteObserverBridge.get());
    _autocompleteObserverBridge.reset();
  }
}

- (void)setConsumer:(id<OmniboxDebuggerConsumer,
                        RemoteSuggestionsServiceObserver,
                        AutocompleteControllerObserver>)consumer {
  DCHECK(experimental_flags::IsOmniboxDebuggingEnabled());

  _autocompleteObserverBridge =
      std::make_unique<AutocompleteControllerObserverBridge>(self);
  _autocompleteController->AddObserver(_autocompleteObserverBridge.get());

  // Observe the remote suggestions service if it's available. It might not
  // be available e.g. in incognito.
  if (_remoteSuggestionsService) {
    _remoteSuggestionsServiceObserverBridge =
        std::make_unique<RemoteSuggestionsServiceObserverBridge>(
            self, _remoteSuggestionsService);
    _remoteSuggestionsService->AddObserver(
        _remoteSuggestionsServiceObserverBridge.get());
  }

  _consumer = consumer;
}

#pragma mark - OmniboxDebuggerMutator

- (void)hardcodeSuggestResponse:(NSString*)response {
  _hardcodedSuggestResponse = response.cr_UTF8String;

  if (!_remoteSuggestionsServiceDelegateBridge && _remoteSuggestionsService) {
    _remoteSuggestionsServiceDelegateBridge =
        std::make_unique<RemoteSuggestionsServiceDelegateBridge>(
            self, _remoteSuggestionsService);
    _remoteSuggestionsService->SetDelegate(
        _remoteSuggestionsServiceDelegateBridge->AsWeakPtr());
  }
}

#pragma mark - RemoteSuggestionsServiceDelegate

- (void)onRequestCompleted:(const network::SimpleURLLoader*)source
              responseCode:(int)responseCode
              responseBody:(std::optional<std::string>)responseBody
                completion:
                    (RemoteSuggestionsService::CompletionCallback)completion {
  if (responseCode == 200 && !_hardcodedSuggestResponse.empty()) {
    *responseBody = _hardcodedSuggestResponse;
  }

  std::move(completion).Run(source, responseCode, std::move(responseBody));
}

- (void)onIndexedRequestCompleted:(int)requestIndex
                        urlLoader:(const network::SimpleURLLoader*)source
                     responseCode:(int)responseCode
                     responseBody:(std::optional<std::string>)responseBody
                       completion:
                           (RemoteSuggestionsService::IndexedCompletionCallback)
                               completion {
  if (responseCode == 200 && !_hardcodedSuggestResponse.empty()) {
    *responseBody = _hardcodedSuggestResponse;
  }

  std::move(completion)
      .Run(requestIndex, source, responseCode, std::move(responseBody));
}

#pragma mark - RemoteSuggestionsServiceObserver

- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
    createdRequestWithIdentifier:
        (const base::UnguessableToken&)requestIdentifier
                         request:(const network::ResourceRequest*)request {
  OmniboxRemoteSuggestionEvent* event = [[OmniboxRemoteSuggestionEvent alloc]
      initWithUniqueIdentifier:requestIdentifier];
  event.requestURL = [NSString cr_fromString:request->url.spec()];

  [self.consumer registerNewOmniboxEvent:event];
}

- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
    startedRequestWithIdentifier:
        (const base::UnguessableToken&)requestIdentifier
                     requestBody:(NSString*)requestBody
                       URLLoader:(network::SimpleURLLoader*)URLLoader {
  [self.consumer
      updateRemoteSuggestionEventWithRequestIdentifier:requestIdentifier
                                           requestBody:requestBody];
}

- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
    completedRequestWithIdentifier:
        (const base::UnguessableToken&)requestIdentifier
                      responseCode:(NSInteger)code
                      responseBody:(NSString*)responseBody {
  [self.consumer
      updateRemoteSuggestionEventWithRequestIdentifier:requestIdentifier
                                          responseBody:responseBody
                                          responseCode:code];
}

#pragma mark - AutocompleteControllerObserver

- (void)autocompleteController:(AutocompleteController*)controller
             didStartWithInput:(const AutocompleteInput&)input {
}

- (void)autocompleteController:(AutocompleteController*)controller
    didUpdateResultChangingDefaultMatch:(BOOL)defaultMatchChanged {
  OmniboxAutocompleteEvent* event = [[OmniboxAutocompleteEvent alloc]
      initWithAutocompleteController:controller];

  [self.consumer registerNewOmniboxEvent:event];
}

#pragma mark - OmniboxAutocompleteControllerDebuggerDelegate

- (void)omniboxAutocompleteController:
            (OmniboxAutocompleteController*)autocompleteController
    didUpdateWithSuggestionsAvailable:(BOOL)hasSuggestions {
  [self.consumer
      setVariationIDString:
          [NSString
              cr_fromString:variations::VariationsIdsProvider::GetInstance()
                                ->GetTriggerVariationsString()]];

  if (!hasSuggestions) {
    [self.consumer removeAllObjects];
  }
}

@end
