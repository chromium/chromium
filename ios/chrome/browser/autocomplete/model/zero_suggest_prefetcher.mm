// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/zero_suggest_prefetcher.h"

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

@interface ZeroSuggestPrefetcher () <CRWWebStateObserver, WebStateListObserving>
@end

@implementation ZeroSuggestPrefetcher {
  raw_ptr<AutocompleteController> _autocompleteController;
  // Observed web state list. Only used when observing a web state list.
  raw_ptr<WebStateList> _webStateList;
  // Observed web state. Only used in singular web state observation mode.
  raw_ptr<web::WebState> _webState;
  base::RepeatingCallback<metrics::OmniboxEventProto::PageClassification()>
      _classificationCallback;
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  // Callback to cleanup parents reference to the webState / webStateList.
  base::OnceCallback<void(web::WebState*)> _webStateDisconnectCallback;
  base::OnceCallback<void(WebStateList*)> _webStateListDisconnectCallback;
}

- (instancetype)
    initWithAutocompleteController:
        (AutocompleteController*)autocompleteController
                      webStateList:(WebStateList*)webStateList
            classificationCallback:
                (base::RepeatingCallback<
                    metrics::OmniboxEventProto::PageClassification()>)
                    classificationCallback
                disconnectCallback:(base::OnceCallback<void(WebStateList*)>)
                                       disconnectCallback {
  if ((self = [super init])) {
    [self
        commonInitWithAutocompleteController:autocompleteController
                      classificationCallback:std::move(classificationCallback)];
    _webStateList = webStateList;
    _webStateListDisconnectCallback = std::move(disconnectCallback);
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _webStateObserverBridge.get());
    _webStateList->AddObserver(_webStateListObserverBridge.get());
    [self startPrefetch];
  }
  return self;
}

- (instancetype)
    initWithAutocompleteController:
        (AutocompleteController*)autocompleteController
                          webState:(web::WebState*)webState
            classificationCallback:
                (base::RepeatingCallback<
                    metrics::OmniboxEventProto::PageClassification()>)
                    classificationCallback
                disconnectCallback:(base::OnceCallback<void(web::WebState*)>)
                                       disconnectCallback {
  if ((self = [super init])) {
    [self
        commonInitWithAutocompleteController:autocompleteController
                      classificationCallback:std::move(classificationCallback)];
    _webState = webState;
    _webStateDisconnectCallback = std::move(disconnectCallback);
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    [self startPrefetch];
  }
  return self;
}

- (void)commonInitWithAutocompleteController:
            (AutocompleteController*)autocompleteController
                      classificationCallback:
                          (base::RepeatingCallback<
                              metrics::OmniboxEventProto::PageClassification()>)
                              classificationCallback {
  _autocompleteController = autocompleteController;
  _classificationCallback = std::move(classificationCallback);
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(appWillEnterForeground)
             name:UIApplicationWillEnterForegroundNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(appDidEnterBackground)
             name:UIApplicationDidEnterBackgroundNotification
           object:nil];
}

- (void)disconnect {
  _webStateListDisconnectCallback.Reset();
  _webStateDisconnectCallback.Reset();
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserverBridge.get());
    _activeWebStateObservationForwarder.reset();
    _webStateListObserverBridge.reset();
    _webStateList = nullptr;
  }
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webState = nullptr;
  }
  _webStateObserverBridge.reset();
  if (_autocompleteController) {
    _autocompleteController = nullptr;
  }
  _classificationCallback.Reset();
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change() && status.new_active_web_state) {
    [self startPrefetch];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  if (_webStateListDisconnectCallback) {
    std::move(_webStateListDisconnectCallback).Run(_webStateList);
  }
  [self disconnect];
}

#pragma mark - web::WebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  if (_webStateDisconnectCallback) {
    std::move(_webStateDisconnectCallback).Run(_webState);
  }
  [self disconnect];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  [self startPrefetch];
}

#pragma mark - Private

/// Indicates autocomplete that the app has entered a foreground state.
- (void)appWillEnterForeground {
  if (_autocompleteController) {
    _autocompleteController->autocomplete_provider_client()
        ->set_in_background_state(false);
  }
  [self startPrefetch];
}

/// Indicates autocomplete that the app has entered a background state.
- (void)appDidEnterBackground {
  if (_autocompleteController) {
    _autocompleteController->autocomplete_provider_client()
        ->set_in_background_state(true);
  }
}

/// Tell autocomplete to start prefetching whenever there's an active web state.
/// The prefetching is expected on navigation, tab switch, and page reload.
- (void)startPrefetch {
  if (!_autocompleteController || !_classificationCallback) {
    return;
  }
  web::WebState* active_web_state = nullptr;
  if (_webStateList) {
    active_web_state = _webStateList->GetActiveWebState();
  } else if (_webState) {
    active_web_state = _webState;
  }
  if (!active_web_state) {
    return;
  }
  GURL current_url = active_web_state->GetVisibleURL();
  metrics::OmniboxEventProto::PageClassification page_classification =
      _classificationCallback.Run();

  std::u16string text = base::UTF8ToUTF16(current_url.spec());
  if (omnibox::IsNTPPage(page_classification)) {
    text.clear();
  }

  AutocompleteInput input(
      text, page_classification,
      _autocompleteController->autocomplete_provider_client()
          ->GetSchemeClassifier());
  input.set_current_url(current_url);
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  _autocompleteController->StartPrefetch(input);
}

@end
