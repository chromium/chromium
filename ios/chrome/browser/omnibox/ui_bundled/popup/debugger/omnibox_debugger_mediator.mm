// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui_bundled/popup/debugger/omnibox_debugger_mediator.h"

#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/remote_suggestions_service.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/debugger/autocomplete_controller_observer_bridge.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/debugger/popup_debug_info_consumer.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/debugger/remote_suggestions_service_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

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

  if (_autocompleteObserverBridge) {
    _autocompleteController->RemoveObserver(_autocompleteObserverBridge.get());
    _autocompleteObserverBridge.reset();
  }
}

- (void)setConsumer:(id<PopupDebugInfoConsumer,
                        RemoteSuggestionsServiceObserver,
                        AutocompleteControllerObserver>)consumer {
  DCHECK(experimental_flags::IsOmniboxDebuggingEnabled());

  _autocompleteObserverBridge =
      std::make_unique<AutocompleteControllerObserverBridge>(consumer);
  _autocompleteController->AddObserver(_autocompleteObserverBridge.get());

  // Observe the remote suggestions service if it's available. It might not
  // be available e.g. in incognito.
  if (_remoteSuggestionsService) {
    _remoteSuggestionsServiceObserverBridge =
        std::make_unique<RemoteSuggestionsServiceObserverBridge>(
            consumer, _remoteSuggestionsService);
    _remoteSuggestionsService->AddObserver(
        _remoteSuggestionsServiceObserverBridge.get());
  }

  _consumer = consumer;
}

#pragma mark - OmniboxAutocompleteControllerDebuggerDelegate

- (void)omniboxAutocompleteController:
            (OmniboxAutocompleteController*)autocompleteController
    didUpdateWithSuggestionsAvailable:(BOOL)hasSuggestions {
  [self.consumer
      setVariationIDString:base::SysUTF8ToNSString(
                               variations::VariationsIdsProvider::GetInstance()
                                   ->GetTriggerVariationsString())];

  if (!hasSuggestions) {
    [self.consumer removeAllObjects];
  }
}

@end
