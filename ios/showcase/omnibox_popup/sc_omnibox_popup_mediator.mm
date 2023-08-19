// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_mediator.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_result_consumer.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/showcase/omnibox_popup/fake_autocomplete_suggestion.h"
#import "url/gurl.h"

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

  AutocompleteSuggestionGroupImpl* group =
      [AutocompleteSuggestionGroupImpl groupWithTitle:nil
                                          suggestions:suggestions];

  [self.consumer updateMatches:@[ group ] preselectedMatchGroupIndex:0];
}

@end
