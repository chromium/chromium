// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_inline_worklog_view_provider.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actor_task_data_source.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_entries_diff.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_suggestions_presenter.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actuation_worklog_builders.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_block_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_actuations_block.h"

namespace {

// What an inline worklog view shows.
enum class InlineWorklogContent {
  // Live data, current or final. The view keeps it.
  kLive,
  // Nothing yet: the running task's live data, once its data source is bound.
  kPending,
  // The data of its block.
  kBlock,
};

// Returns the agent task carried by `block`, or nil if `block` holds another
// type of block.
id<GeminiBrowserActuationsBlock> BrowserActuationsBlock(
    id<GeminiBlockNode> block) {
  if (![block conformsToProtocol:@protocol(GeminiBrowserActuationsBlockNode)]) {
    return nil;
  }
  return static_cast<id<GeminiBrowserActuationsBlockNode>>(block)
      .browserActuationsBlock;
}

// Returns the items of `entries`.
NSArray<ActuationWorklogItem*>* ItemsOfEntries(
    NSArray<ActuationWorklogEntry*>* entries) {
  NSMutableArray<ActuationWorklogItem*>* items =
      [NSMutableArray arrayWithCapacity:entries.count];
  for (ActuationWorklogEntry* entry in entries) {
    [items addObject:entry.item];
  }
  return items;
}

// Returns whether `state` and `other_state` display the same items. Only
// compares titles and styles: the other fields of items built from a block
// are fixed for a given title and style.
bool HaveSameItems(ActuationWorklogState* state,
                   ActuationWorklogState* other_state) {
  if (state.entries.count != other_state.entries.count) {
    return false;
  }
  for (NSUInteger i = 0; i < state.entries.count; ++i) {
    ActuationWorklogItem* item = state.entries[i].item;
    ActuationWorklogItem* other_item = other_state.entries[i].item;
    if (item.style != other_item.style ||
        ![item.title isEqualToString:other_item.title]) {
      return false;
    }
  }
  return true;
}

// Returns whether `context` is the latest response and it is still streaming.
bool IsLatestStreamingResponse(id<GeminiBlockContext> context) {
  return context.isLatestResponse &&
         context.responseState == GeminiResponseState::kStreaming;
}

// Returns a new worklog view for a response.
ActuationWorklogView* CreateWorklogView() {
  ActuationWorklogView* view =
      [[ActuationWorklogView alloc] initWithFrame:CGRectZero];
  // Past worklogs start collapsed to keep the conversation compact.
  view.collapsed = YES;
  // The SDK resizes the cell right away while collapsing items fade out, so
  // they would draw over the next cell.
  view.clipsToBounds = YES;
  return view;
}

}  // namespace

@interface GeminiInlineWorklogViewProvider () <ActorTaskDataSourceObserver,
                                               ActuationWorklogViewDelegate>
@end

@implementation GeminiInlineWorklogViewProvider {
  // Shows the picker of a tapped suggestion card.
  __weak id<ActuationSuggestionsPresenter> _suggestionsPresenter;
  // State last applied to each provided view. Keys are weak because the SDK
  // owns the views.
  NSMapTable<ActuationWorklogView*, ActuationWorklogState*>* _appliedStates;
  // Data source of the running task, if any.
  __weak id<ActorTaskDataSource> _dataSource;
  // View of the running task's response, if any. Follows `_dataSource` if set,
  // and otherwise stays empty until it arrives.
  __weak ActuationWorklogView* _runningTaskView;
  // Views that followed a data source. They keep their live content, which is
  // richer than their block's.
  NSHashTable<ActuationWorklogView*>* _liveViews;
}

- (instancetype)initWithSuggestionsPresenter:
    (id<ActuationSuggestionsPresenter>)suggestionsPresenter {
  self = [super init];
  if (self) {
    _suggestionsPresenter = suggestionsPresenter;
    _appliedStates = [NSMapTable weakToStrongObjectsMapTable];
    _liveViews = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

#pragma mark - Public

- (void)startObservingDataSource:(id<ActorTaskDataSource>)dataSource {
  // A new task replaces the running one without it being stopped first.
  if (_dataSource) {
    [self stopObservingDataSource];
  }
  _dataSource = dataSource;
  // The running task's view may have arrived first.
  [self bindRunningTaskViewIfNeeded];
}

- (void)stopObservingDataSource {
  [_dataSource removeObserver:self];
  _dataSource = nil;
  // The task may stop without publishing a final state (e.g. when replaced),
  // so its last step must no longer look in progress.
  ActuationWorklogView* runningTaskView = _runningTaskView;
  runningTaskView.actuating = NO;
  [runningTaskView setChip:nil];
  // Without any live state (e.g. the task never registered), the view would
  // stay empty, so it falls back to its block instead.
  if (runningTaskView && ![_appliedStates objectForKey:runningTaskView]) {
    [_liveViews removeObject:runningTaskView];
  }
  _runningTaskView = nil;
}

#pragma mark - GeminiCustomUIViewProvider

- (BOOL)providesViewForBlock:(id<GeminiBlockNode>)block {
  return BrowserActuationsBlock(block) != nil;
}

- (BOOL)hidesBlock:(id<GeminiBlockNode>)block {
  return NO;
}

- (UIView*)viewForBlock:(id<GeminiBlockNode>)block
                context:(id<GeminiBlockContext>)context
            reusingView:(UIView*)view {
  ActuationWorklogView* worklogView =
      base::apple::ObjCCast<ActuationWorklogView>(view);
  if (!worklogView) {
    worklogView = CreateWorklogView();
  }
  // A reused view may come from a previous provider.
  worklogView.delegate = self;
  switch ([self contentOfView:worklogView inContext:context]) {
    case InlineWorklogContent::kLive:
      // Live content is richer than the block's, so the view is left as is.
      break;
    case InlineWorklogContent::kPending:
      _runningTaskView = worklogView;
      [self bindRunningTaskViewIfNeeded];
      break;
    case InlineWorklogContent::kBlock:
      // The response finished without its task reporting a data source.
      if (_runningTaskView == worklogView) {
        _runningTaskView = nil;
      }
      [self showBlock:block inView:worklogView];
      break;
  }
  return worklogView;
}

#pragma mark - ActorTaskDataSourceObserver

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
        didUpdateState:(ActuationWorklogState*)state {
  ActuationWorklogView* runningTaskView = _runningTaskView;
  // The data source has no state until its task registers.
  if (dataSource != _dataSource || !runningTaskView || !state) {
    return;
  }
  ActuationWorklogState* appliedState =
      [_appliedStates objectForKey:runningTaskView];
  ActuationWorklogEntriesDiff diff =
      ComputeActuationWorklogEntriesDiff(appliedState.entries, state.entries);
  // Set first so the trailing item's progress styling matches `state`.
  runningTaskView.actuating = state.actuationActive;
  if (!appliedState) {
    // Expands only with live content, so a block fallback stays collapsed.
    runningTaskView.collapsed = NO;
  }
  if (!appliedState || diff.reset) {
    [runningTaskView setItems:ItemsOfEntries(diff.appended)];
  } else {
    if (diff.replaced_last) {
      [runningTaskView setLastItem:diff.replaced_last.item];
    }
    for (ActuationWorklogEntry* entry in diff.appended) {
      [runningTaskView addItem:entry.item];
    }
  }
  [runningTaskView setChip:state.entries.lastObject.chip];
  [_appliedStates setObject:state forKey:runningTaskView];
}

#pragma mark - ActuationWorklogViewDelegate

- (void)worklogView:(ActuationWorklogView*)worklogView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem {
  // The presenter ignores cards that aren't the running task's current one,
  // e.g. those of past tasks.
  [_suggestionsPresenter showSuggestionsPickerForAccessory:accessoryItem];
}

#pragma mark - Private

// Returns what `view` shows in `context`. Doesn't change any state.
- (InlineWorklogContent)contentOfView:(ActuationWorklogView*)view
                            inContext:(id<GeminiBlockContext>)context {
  if ([_liveViews containsObject:view]) {
    return InlineWorklogContent::kLive;
  }
  // Only the latest streaming response can be running a task, and only one
  // view follows it.
  if (!IsLatestStreamingResponse(context) ||
      (_runningTaskView && _runningTaskView != view)) {
    return InlineWorklogContent::kBlock;
  }
  return InlineWorklogContent::kPending;
}

// Fills `view` from `block`, unless it already shows the same items.
- (void)showBlock:(id<GeminiBlockNode>)block
           inView:(ActuationWorklogView*)view {
  ActuationWorklogState* state = ActuationWorklogStateForBrowserActuationsBlock(
      BrowserActuationsBlock(block));
  // The SDK asks again on every layout change (e.g. collapsing, or each frame
  // while the response streams), so the view is only rebuilt when the block
  // shows different items.
  ActuationWorklogState* appliedState = [_appliedStates objectForKey:view];
  if (appliedState && HaveSameItems(state, appliedState)) {
    return;
  }
  view.actuating = state.actuationActive;
  [view setItems:ItemsOfEntries(state.entries)];
  [view setChip:state.entries.lastObject.chip];
  [_appliedStates setObject:state forKey:view];
}

// Makes `_runningTaskView` follow `_dataSource` once both are known.
- (void)bindRunningTaskViewIfNeeded {
  ActuationWorklogView* runningTaskView = _runningTaskView;
  if (!runningTaskView || !_dataSource) {
    return;
  }
  [_liveViews addObject:runningTaskView];
  // A reused view may hold the state of the block it showed before.
  [_appliedStates removeObjectForKey:runningTaskView];
  // The data source replays its current state right away.
  [_dataSource addObserver:self];
}

@end
