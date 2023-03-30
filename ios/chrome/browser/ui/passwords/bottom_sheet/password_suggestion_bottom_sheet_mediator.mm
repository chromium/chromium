// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordSuggestionBottomSheetMediator () <WebStateListObserving,
                                                     CRWWebStateObserver>

// The object that provides suggestions while filling forms.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> suggestionsProvider;

// List of suggestions in the bottom sheet.
@property(nonatomic, strong) NSArray<FormSuggestion*>* suggestions;

@end

@implementation PasswordSuggestionBottomSheetMediator {
  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;

  // Whether the field that triggered the bottom sheet will need to refocus when
  // the bottom sheet is dismissed. Default is true.
  bool _needsRefocus;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                              params:
                                  (const autofill::FormActivityParams&)params {
  if (self = [super init]) {
    _needsRefocus = true;
    _webStateList = webStateList;
    web::WebState* activeWebState = _webStateList->GetActiveWebState();

    // Create and register the observers.
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        webStateList, _observer.get());

    if (activeWebState) {
      FormSuggestionTabHelper* tabHelper =
          FormSuggestionTabHelper::FromWebState(activeWebState);
      DCHECK(tabHelper);

      self.suggestionsProvider = tabHelper->GetAccessoryViewProvider();
      DCHECK(self.suggestionsProvider);

      [self.suggestionsProvider
          retrieveSuggestionsForForm:params
                            webState:activeWebState
            accessoryViewUpdateBlock:^(
                NSArray<FormSuggestion*>* suggestions,
                id<FormInputSuggestionsProvider> formInputSuggestionsProvider) {
              self.suggestions = suggestions;
            }];
    }
  }
  return self;
}

- (void)dealloc {
}

- (void)disconnect {
  _webStateList = nullptr;
  _forwarder = nullptr;
  _observer = nullptr;
}

#pragma mark - Accessors

- (void)setConsumer:(id<PasswordSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;
  if ([self.suggestions count] > 0) {
    [consumer setSuggestions:self.suggestions];
  } else {
    [consumer dismiss];
  }
}

#pragma mark - PasswordSuggestionBottomSheetDelegate

- (void)didSelectSuggestion:(NSInteger)row {
  DCHECK(row >= 0);

  _needsRefocus = false;
  FormSuggestion* suggestion = [self.suggestions objectAtIndex:row];
  [self.suggestionsProvider didSelectSuggestion:suggestion];
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_EQ(_webStateList, webStateList);
  if (atIndex == webStateList->active_index()) {
    _needsRefocus = false;
    [self.consumer dismiss];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(_webStateList, webStateList);
  _needsRefocus = false;
  [self.consumer dismiss];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  _needsRefocus = false;
  [self.consumer dismiss];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  _needsRefocus = false;
  [self.consumer dismiss];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  _needsRefocus = false;
  [self.consumer dismiss];
}

@end
