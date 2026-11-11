// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import "ios/chrome/browser/intelligence/actor/coordinator/actor_task_data_source.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"

namespace {

// Returns the number of leading entries `entries` and `previousEntries` share.
NSUInteger SharedPrefixLength(
    NSArray<ActuationWorklogEntry*>* entries,
    NSArray<ActuationWorklogEntry*>* previousEntries) {
  NSUInteger length = 0;
  NSUInteger maxLength = MIN(entries.count, previousEntries.count);
  while (length < maxLength && entries[length] == previousEntries[length]) {
    ++length;
  }
  return length;
}

// Returns whether `entries` can update `previousEntries` in place by
// optionally replacing the last entry and appending the rest.
BOOL CanUpdateEntriesInPlace(NSArray<ActuationWorklogEntry*>* entries,
                             NSArray<ActuationWorklogEntry*>* previousEntries) {
  NSUInteger previousCount = previousEntries.count;
  return entries.count >= previousCount &&
         SharedPrefixLength(entries, previousEntries) + 1 >= previousCount;
}

}  // namespace

@interface ActuationWorklogMediator () <ActorTaskDataSourceObserver>
@end

@implementation ActuationWorklogMediator {
  // Whether a data source is observed. Tracked separately because
  // `_dataSource` is weak.
  BOOL _observing;
  // Source of the worklog of the observed task.
  __weak id<ActorTaskDataSource> _dataSource;
  // Handler of the user actions on the worklog of the observed task.
  __weak id<ActorTaskActionHandler> _actionHandler;
  // Latest applied state.
  ActuationWorklogState* _state;
}

#pragma mark - Public

- (void)startObservingDataSource:(id<ActorTaskDataSource>)dataSource
                   actionHandler:(id<ActorTaskActionHandler>)actionHandler {
  if (_observing && _dataSource == dataSource) {
    return;
  }
  [self stopObservingDataSource];

  _observing = YES;
  _actionHandler = actionHandler;
  _dataSource = dataSource;
  // Synchronously delivers the current state.
  [_dataSource addObserver:self];
}

- (void)stopObservingDataSource {
  if (!_observing) {
    return;
  }
  [_dataSource removeObserver:self];
  _dataSource = nil;
  _actionHandler = nil;
  _observing = NO;
  _state = nil;
  [self.consumer setActuationActive:NO];
  [self.consumer reset];
  [self.suggestionsConsumer setSuggestions:nil forActiveWorklogItem:nil];
}

- (void)disconnect {
  [self stopObservingDataSource];
  self.consumer = nil;
  self.suggestionsConsumer = nil;
}

#pragma mark - Private

// Sends `state` to consumers (which dedupe unchanged values). Diffs entries
// since the worklog consumer only appends or replaces the last entry,
// re-displaying from scratch when in-place updates aren't possible.
- (void)applyState:(ActuationWorklogState*)state {
  NSArray<ActuationWorklogEntry*>* displayedEntries = _state.entries;
  // Replayed entries are not animated, as the compact view would otherwise play
  // a transition for each past step.
  BOOL animated = _state != nil;
  if (!CanUpdateEntriesInPlace(state.entries, displayedEntries)) {
    [self.consumer reset];
    displayedEntries = nil;
    animated = NO;
  }
  _state = state;

  [self.consumer setActuationActive:state.actuationActive];
  [self.consumer setHeaderContentVariants:state.headerContent];
  [self updateEntries:state.entries
      previousEntries:displayedEntries
             animated:animated];
  [self.consumer setIntervention:state.intervention];
  [self.suggestionsConsumer setSuggestions:state.suggestions
                      forActiveWorklogItem:state.suggestionsEntry.item];
}

// Sends the entries that differ from `previousEntries` to the consumer: the
// replaced last displayed entry, if any, then the appended ones.
- (void)updateEntries:(NSArray<ActuationWorklogEntry*>*)entries
      previousEntries:(NSArray<ActuationWorklogEntry*>*)previousEntries
             animated:(BOOL)animated {
  NSUInteger previousCount = previousEntries.count;
  for (NSUInteger i = SharedPrefixLength(entries, previousEntries);
       i < entries.count; ++i) {
    ActuationWorklogEntry* entry = entries[i];
    // The consumer only displays the chip of the latest entry.
    ActuationWorklogChip* chip = entry == entries.lastObject ? entry.chip : nil;
    if (i < previousCount) {
      [self.consumer setLastItem:entry.item chip:chip];
    } else {
      [self.consumer updateWorklogWithItem:entry.item
                                      chip:chip
                                  animated:animated];
    }
  }
}

#pragma mark - ActorTaskDataSourceObserver

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
        didUpdateState:(ActuationWorklogState*)state {
  // Nothing is displayed until the task is registered.
  if (!state) {
    return;
  }
  [self applyState:state];
}

#pragma mark - ActuationWorklogMutator

- (void)stopActuation {
  [_actionHandler stopTask];
}

- (void)didTriggerInterventionAction:(ActuationInterventionAction*)action {
  [_actionHandler resolveInterventionWithAction:action];
}

#pragma mark - ActuationSuggestionsDelegate

- (void)suggestionsCoordinator:(ActuationSuggestionsCoordinator*)coordinator
           didSelectSuggestion:(ActorFormSuggestion*)suggestion {
  [_actionHandler selectSuggestion:suggestion];
}

@end
