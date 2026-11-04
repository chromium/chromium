// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import <optional>

#import "base/callback_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"

namespace {

// Converts a `TTCJournalFilter` to an optional `TTCJournalComponent`.
std::optional<TTCJournalComponent> JournalComponentForFilter(
    TTCJournalFilter filter) {
  switch (filter) {
    case TTCJournalFilter::kAll:
      return std::nullopt;
    case TTCJournalFilter::kSession:
      return TTCJournalComponent::kSession;
    case TTCJournalFilter::kConversation:
      return TTCJournalComponent::kConversation;
    case TTCJournalFilter::kBackend:
      return TTCJournalComponent::kBackend;
    case TTCJournalFilter::kAudio:
      return TTCJournalComponent::kAudio;
    case TTCJournalFilter::kActuation:
      return TTCJournalComponent::kActuation;
  }
}

}  // namespace

@implementation TTCMediator {
  // TTC keyed service for the profile.
  raw_ptr<TTCKeyedService> _ttcService;

  // Subscription observing entries added to TTCJournal.
  base::CallbackListSubscription _journalEntryAddedSubscription;

  // Subscription observing entries updated in-place in TTCJournal.
  base::CallbackListSubscription _journalEntryUpdatedSubscription;

  // Subscription observing when TTCJournal is cleared.
  base::CallbackListSubscription _journalClearedSubscription;

  // Optional component filter applied to displayed journal lines.
  std::optional<TTCJournalComponent> _journalComponentFilter;

  // Whether the journal displays full detailed telemetry (`YES`) or simplified
  // human-readable lines. Defaults to NO.
  BOOL _journalDetailedMode;
}

#pragma mark - Initialization

- (instancetype)initWithTTCService:(TTCKeyedService*)ttcService {
  self = [super init];
  if (self) {
    _ttcService = ttcService;
    _journalDetailedMode = NO;
    if (_ttcService) {
      __weak TTCMediator* weakSelf = self;
      _journalEntryAddedSubscription =
          _ttcService->journal().AddEntryAddedListener(
              base::BindRepeating(^(const TTCJournalEntry& entry) {
                [weakSelf handleJournalEntryAdded:entry];
              }));
      _journalEntryUpdatedSubscription =
          _ttcService->journal().AddEntryUpdatedListener(
              base::BindRepeating(^(const TTCJournalEntry& entry) {
                [weakSelf handleJournalEntryUpdated:entry];
              }));
      _journalClearedSubscription =
          _ttcService->journal().AddClearedListener(base::BindRepeating(^{
            [weakSelf handleJournalCleared];
          }));
    }
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<TTCConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self hydrateConsumer];
  }
}

#pragma mark - TTCMutator

- (void)viewWillAppear {
  [self hydrateConsumer];
}

- (void)clearJournal {
  if (_ttcService) {
    _ttcService->journal().Clear();
  }
}

- (void)setJournalComponentFilter:(TTCJournalFilter)filter {
  _journalComponentFilter = JournalComponentForFilter(filter);
  [self.consumer setJournalLines:[self formattedJournalLines]];
}

- (void)setJournalDetailedMode:(BOOL)detailed {
  _journalDetailedMode = detailed;
  [self.consumer setJournalLines:[self formattedJournalLines]];
}

#pragma mark - Public

- (void)disconnect {
  _journalEntryAddedSubscription = {};
  _journalEntryUpdatedSubscription = {};
  _journalClearedSubscription = {};
  _ttcService = nullptr;
  _consumer = nil;
}

#pragma mark - Private

// Formats a single `TTCJournalEntry` according to `_journalDetailedMode`,
// returning `nil` if the entry should be omitted in simplified mode.
- (NSString*)formatJournalEntry:(const TTCJournalEntry&)entry {
  std::string formatted =
      _journalDetailedMode ? entry.FormatLine() : entry.FormatSimplifiedLine();
  if (formatted.empty()) {
    return nil;
  }
  return base::SysUTF8ToNSString(formatted);
}

// Handles a new entry appended to `TTCJournal`.
- (void)handleJournalEntryAdded:(const TTCJournalEntry&)entry {
  if (_journalComponentFilter.has_value() &&
      entry.component != *_journalComponentFilter) {
    return;
  }
  if (NSString* line = [self formatJournalEntry:entry]) {
    [self.consumer appendJournalLine:line];
  }
}

// Handles an existing entry updated in-place in `TTCJournal`.
- (void)handleJournalEntryUpdated:(const TTCJournalEntry&)entry {
  if (_journalComponentFilter.has_value() &&
      entry.component != *_journalComponentFilter) {
    return;
  }
  [self.consumer setJournalLines:[self formattedJournalLines]];
}

// Handles `TTCJournal::Clear()` notifications.
- (void)handleJournalCleared {
  [self.consumer setJournalLines:@[]];
}

// Returns buffered journal lines matching `_journalComponentFilter`.
- (NSArray<NSString*>*)formattedJournalLines {
  if (!_ttcService) {
    return @[];
  }
  NSMutableArray<NSString*>* lines = [[NSMutableArray alloc] init];
  for (const TTCJournalEntry& entry : _ttcService->journal().entries()) {
    if (_journalComponentFilter.has_value() &&
        entry.component != *_journalComponentFilter) {
      continue;
    }
    if (NSString* line = [self formatJournalEntry:entry]) {
      [lines addObject:line];
    }
  }
  return lines;
}

// Hydrates the consumer with current journal lines.
- (void)hydrateConsumer {
  if (!self.consumer) {
    return;
  }
  [self.consumer setJournalLines:[self formattedJournalLines]];
}

@end
