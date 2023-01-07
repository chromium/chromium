// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/zero_suggest_prefetch_helper.h"

#import "base/feature_list.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::WebState;
using web::WebStateObserverBridge;

@interface ZeroSuggestPrefetchHelper () <CRWWebStateObserver>
@end

@implementation ZeroSuggestPrefetchHelper {
  /// Bridge to receive active web state events
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;
  /// Bridge to receive WS events for the active web state.
  std::unique_ptr<WebStateObserverBridge> _webStateObserverBridge;
}

- (void)dealloc {
  /// Reset the web state observation forwarder, which will remove
  /// `_webStateObserverBridge` from the relevant observer list.
  _activeWebStateObservationForwarder.reset();
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
              autocompleteController:
                  (AutocompleteController*)autocompleteController {
  self = [super init];
  if (self) {
    DCHECK(webStateList);
    DCHECK(autocompleteController);
    DCHECK(base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetching));

    _webStateList = webStateList;
    _autocompleteController = autocompleteController;
    _webStateObserverBridge = std::make_unique<WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _webStateObserverBridge.get());
    [self startPrefetchIfNecessary];
  }
  return self;
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  [self startPrefetchIfNecessary];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  [self startPrefetchIfNecessary];
}

#pragma mark - private

+ (BOOL)isNTPURL:(GURL)url {
  return url == kChromeUINewTabURL || url == kChromeUIAboutNewTabURL;
}

/// Start prefetching if the active web state is displaying an NTP.
- (void)startPrefetchIfNecessary {
  WebState* activeWebState = _webStateList->GetActiveWebState();
  if (activeWebState == nullptr ||
      ![ZeroSuggestPrefetchHelper
          isNTPURL:activeWebState->GetLastCommittedURL()]) {
    return;
  }

  AutocompleteInput autocomplete_input(
      u"", metrics::OmniboxEventProto::NTP_ZPS_PREFETCH,
      AutocompleteSchemeClassifierImpl());
  autocomplete_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);
  self.autocompleteController->StartPrefetch(autocomplete_input);
}

@end
