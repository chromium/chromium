// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"

#include "base/check.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_suggestion_wrapper.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_match_preview_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// How many pedals can be shown at once. This number will be displayed in a
// separate section, the rest are ignored.
const NSUInteger kMaxPedalCount = 1;

// Only this many suggestions are considered when extracting pedals. E.g. if
// there is 100 suggestions with pedals, only the first kMaxPedalExtractionRow
// are used to extract pedals.
const NSUInteger kMaxPedalExtractionRow = 3;

}  // namespace

@interface PedalSectionExtractor ()

@property(nonatomic, strong)
    NSMutableArray<id<OmniboxPedal, OmniboxIcon>>* extractedPedals;
@property(nonatomic, strong)
    NSArray<id<AutocompleteSuggestionGroup>>* originalResult;
@property(nonatomic, assign) NSInteger highlightedPedalIndex;

@end

@implementation PedalSectionExtractor

- (instancetype)init {
  self = [super init];
  if (self) {
    _extractedPedals = [[NSMutableArray alloc] init];
    _highlightedPedalIndex = NSNotFound;
  }
  return self;
}

#pragma mark - AutocompleteResultConsumer

- (void)updateMatches:(NSArray<id<AutocompleteSuggestionGroup>>*)result
        withAnimation:(BOOL)animation {
  [self.extractedPedals removeAllObjects];
  self.highlightedPedalIndex = NSNotFound;
  self.originalResult = result;

  for (id<AutocompleteSuggestionGroup> group in result) {
    for (NSUInteger i = 0;
         i < group.suggestions.count && i < kMaxPedalExtractionRow; i++) {
      id<AutocompleteSuggestion> suggestion = group.suggestions[i];

      if (suggestion.pedal != nil) {
        [self.extractedPedals addObject:suggestion.pedal];
      }
    }
  }

  if (self.extractedPedals.count == 0) {
    [self.dataSink updateMatches:self.originalResult withAnimation:animation];
    return;
  }

  while (self.extractedPedals.count > kMaxPedalCount) {
    [self.extractedPedals removeLastObject];
  }

  NSMutableArray* wrappedPedals = [[NSMutableArray alloc] init];
  for (id<OmniboxPedal, OmniboxIcon> pedal in self.extractedPedals) {
    [wrappedPedals
        addObject:[[PedalSuggestionWrapper alloc] initWithPedal:pedal]];
  }

  AutocompleteSuggestionGroupImpl* pedalGroup =
      [AutocompleteSuggestionGroupImpl groupWithTitle:nil
                                          suggestions:wrappedPedals];

  NSArray* combinedGroups = @[ pedalGroup ];
  combinedGroups = [combinedGroups arrayByAddingObjectsFromArray:result];

  [self.dataSink updateMatches:combinedGroups withAnimation:animation];
}

- (void)setTextAlignment:(NSTextAlignment)alignment {
  [self.dataSink setTextAlignment:alignment];
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  [self.dataSink setSemanticContentAttribute:semanticContentAttribute];
}

#pragma mark - AutocompleteResultConsumerDelegate

- (void)autocompleteResultConsumerCancelledHighlighting:
    (id<AutocompleteResultConsumer>)sender {
  self.highlightedPedalIndex = NSNotFound;
  [self.delegate autocompleteResultConsumerCancelledHighlighting:self];
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                   didHighlightRow:(NSUInteger)row
                         inSection:(NSUInteger)section {
  if (self.extractedPedals.count > 0) {
    if (section == 0) {
      id<OmniboxPedal> pedal = self.extractedPedals[row];
      [self.delegate autocompleteResultConsumerCancelledHighlighting:self];

      [self.matchPreviewDelegate
          setPreviewMatchText:[[NSAttributedString alloc]
                                  initWithString:pedal.title]
                        image:nil];

      self.highlightedPedalIndex = row;
      return;
    } else {
      self.highlightedPedalIndex = NSNotFound;
      section -= 1;
    }
  }

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:row
                                  inSection:section];
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                      didSelectRow:(NSUInteger)row
                         inSection:(NSUInteger)section {
  if (self.extractedPedals.count > 0) {
    if (section == 0) {
      id<OmniboxPedal> pedal = self.extractedPedals[row];
      if (pedal.action) {
        pedal.action();
      }
      return;
    } else {
      section -= 1;
    }
  }

  [self.delegate autocompleteResultConsumer:self
                               didSelectRow:row
                                  inSection:section];
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
        didTapTrailingButtonForRow:(NSUInteger)row
                         inSection:(NSUInteger)section {
  if (self.extractedPedals.count > 0) {
    // Pedals do not have trailing buttons.
    DCHECK(section > 0);
    section -= 1;
  }

  [self.delegate autocompleteResultConsumer:self
                 didTapTrailingButtonForRow:row
                                  inSection:section];
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
           didSelectRowForDeletion:(NSUInteger)row
                         inSection:(NSUInteger)section {
  if (self.extractedPedals.count > 0) {
    // Pedals do not support deletion.
    DCHECK(section > 0);
    section -= 1;
  }

  [self.delegate autocompleteResultConsumer:self
                    didSelectRowForDeletion:row
                                  inSection:section];
}

- (void)autocompleteResultConsumerDidScroll:
    (id<AutocompleteResultConsumer>)sender {
  [self.delegate autocompleteResultConsumerDidScroll:self];
}

#pragma mark - OmniboxReturnDelegate

- (void)omniboxReturnPressed:(id)sender {
  if (self.highlightedPedalIndex != NSNotFound) {
    id<OmniboxPedal> pedal = self.extractedPedals[self.highlightedPedalIndex];
    if (pedal.action) {
      pedal.action();
    }
    return;
  }

  [self.acceptDelegate omniboxReturnPressed:sender];
}

@end
