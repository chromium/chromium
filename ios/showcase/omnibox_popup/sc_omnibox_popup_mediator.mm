// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_mediator.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_result_consumer.h"
#import "ios/showcase/omnibox_popup/fake_autocomplete_suggestion.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCOmniboxPopupMediator ()

@property(nonatomic, readonly, weak) id<AutocompleteResultConsumer> consumer;

@end

@implementation SCOmniboxPopupMediator

- (instancetype)initWithConsumer:(id<AutocompleteResultConsumer>)consumer {
  self = [super init];
  if (self) {
    _consumer = consumer;
  }
  return self;
}

// Creates many fake suggestions and passes them along to the
// AutocompleteResultConsumer.
- (void)updateMatches {
  NSArray<id<AutocompleteSuggestion>>* suggestions = @[
    [FakeAutocompleteSuggestion simpleSuggestion],
    [FakeAutocompleteSuggestion suggestionWithDetail],
    [FakeAutocompleteSuggestion clippingSuggestion],
    [FakeAutocompleteSuggestion appendableSuggestion],
    [FakeAutocompleteSuggestion otherTabSuggestion],
    [FakeAutocompleteSuggestion deletableSuggestion],
    [FakeAutocompleteSuggestion stockSuggestion],
    [FakeAutocompleteSuggestion weatherSuggestion],
    [FakeAutocompleteSuggestion definitionSuggestion],
    [FakeAutocompleteSuggestion sunriseSuggestion],
    [FakeAutocompleteSuggestion knowledgeSuggestion],
    [FakeAutocompleteSuggestion sportsSuggestion],
    [FakeAutocompleteSuggestion whenIsSuggestion],
    [FakeAutocompleteSuggestion currencySuggestion],
    [FakeAutocompleteSuggestion translateSuggestion],
    [FakeAutocompleteSuggestion calculatorSuggestion],
    [FakeAutocompleteSuggestion richEntitySuggestion],
  ];

  [self.consumer updateMatches:suggestions withAnimation:YES];
}

@end
