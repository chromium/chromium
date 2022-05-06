// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_suggestion_wrapper.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_match_preview_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Time interval for pedal debouncing. Pedal retrieval is async, use a timer to
// avoid pedal flickering (cf. crbug.com/1316404).
const NSTimeInterval kPedalDebouceTimer = 0.3;

}  // namespace

@interface PedalSectionExtractor ()

@property(nonatomic, strong)
    NSArray<id<OmniboxPedal, OmniboxIcon>>* extractedPedals;
// Timer for pedal debouncing.
@property(nonatomic, strong) NSTimer* removePedalsTimer;
@property(nonatomic, strong)
    NSArray<id<AutocompleteSuggestionGroup>>* originalResult;
@property(nonatomic, assign) NSInteger highlightedPedalIndex;

@end

@implementation PedalSectionExtractor

- (instancetype)init {
  self = [super init];
  if (self) {
    _highlightedPedalIndex = NSNotFound;
  }
  return self;
}

#pragma mark - AutocompleteResultConsumer

- (void)updateMatches:(NSArray<id<AutocompleteSuggestionGroup>>*)result
    preselectedMatchGroupIndex:(NSInteger)groupIndex {
  NSMutableArray* extractedPedals = [[NSMutableArray alloc] init];
  self.highlightedPedalIndex = NSNotFound;
  self.originalResult = result;

  NSInteger totalSuggestionCount = 0;
  for (id<AutocompleteSuggestionGroup> group in result) {
    totalSuggestionCount += group.suggestions.count;
    for (NSUInteger i = 0; i < group.suggestions.count; i++) {
      id<AutocompleteSuggestion> suggestion = group.suggestions[i];

      if (suggestion.pedal != nil) {
        [extractedPedals addObject:suggestion.pedal];
      }
    }
  }

  if (extractedPedals.count == 0 && self.extractedPedals.count > 0 &&
      totalSuggestionCount > 0) {
    // If no pedals, display old pedal for a duration of `kPedalDebouceTimer`
    // with new suggestion. This avoids pedal flickering because the pedal
    // results are async. (cf. crbug.com/1316404).
    [self updateMatchesWithPedals:self.extractedPedals suggestionGroup:result];
    if (!self.removePedalsTimer) {
      self.removePedalsTimer =
          [NSTimer scheduledTimerWithTimeInterval:kPedalDebouceTimer
                                           target:self
                                         selector:@selector(removePedals:)
                                         userInfo:nil
                                          repeats:NO];
    }
    return;
  } else {
    [self.removePedalsTimer invalidate];
    self.removePedalsTimer = nil;
  }

  self.extractedPedals = extractedPedals;

  [self updateMatchesWithPedals:extractedPedals suggestionGroup:result];
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
        for (id<OmniboxPedal> displayedPedal in self.extractedPedals) {
          base::UmaHistogramEnumeration(
              "Omnibox.PedalShown",
              static_cast<OmniboxPedalId>(displayedPedal.type),
              OmniboxPedalId::TOTAL_COUNT);
        }

        base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.Pedal",
                                      static_cast<OmniboxPedalId>(pedal.type),
                                      OmniboxPedalId::TOTAL_COUNT);
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

#pragma mark - Private methods

// Removes pedals from suggestions. This is used to debouce pedal with a timer
// to avoid pedal flickering.
- (void)removePedals:(NSTimer*)timer {
  [self.dataSink updateMatches:self.originalResult
      preselectedMatchGroupIndex:0];

  self.extractedPedals = nil;
  self.removePedalsTimer = nil;
}

// Updates matches in `self.dataSink` with pedals from `extractedPedals` and
// suggestions from `result`.
- (void)updateMatchesWithPedals:
            (NSArray<id<OmniboxPedal, OmniboxIcon>>*)extractedPedals
                suggestionGroup:
                    (NSArray<id<AutocompleteSuggestionGroup>>*)result {
  if (extractedPedals.count == 0) {
    [self.dataSink updateMatches:result preselectedMatchGroupIndex:0];
    return;
  }

  NSMutableArray* wrappedPedals = [[NSMutableArray alloc] init];
  for (id<OmniboxPedal, OmniboxIcon> pedal in extractedPedals) {
    [wrappedPedals
        addObject:[[PedalSuggestionWrapper alloc] initWithPedal:pedal]];
  }

  AutocompleteSuggestionGroupImpl* pedalGroup =
      [AutocompleteSuggestionGroupImpl groupWithTitle:nil
                                          suggestions:wrappedPedals];
  NSArray* combinedGroups = @[ pedalGroup ];
  combinedGroups = [combinedGroups arrayByAddingObjectsFromArray:result];
  const NSInteger suggestionGroupIndexInCombinedGroups = 1;

  [self.dataSink updateMatches:combinedGroups
      preselectedMatchGroupIndex:suggestionGroupIndexInCombinedGroups];
}

@end
