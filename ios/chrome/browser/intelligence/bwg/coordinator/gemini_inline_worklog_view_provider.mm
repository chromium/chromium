// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_inline_worklog_view_provider.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actuation_worklog_builders.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_block_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_actuations_block.h"

namespace {

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

}  // namespace

@implementation GeminiInlineWorklogViewProvider {
  // State last applied to each provided view. Keys are weak because the SDK
  // owns the views.
  NSMapTable<ActuationWorklogView*, ActuationWorklogState*>* _appliedStates;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _appliedStates = [NSMapTable weakToStrongObjectsMapTable];
  }
  return self;
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
    worklogView = [[ActuationWorklogView alloc] initWithFrame:CGRectZero];
    // Past worklogs start collapsed to keep the conversation compact.
    worklogView.collapsed = YES;
    // The SDK resizes the cell right away while collapsing items fade out, so
    // they would draw over the next cell.
    worklogView.clipsToBounds = YES;
  }

  // TODO(crbug.com/585107999): Keep the running task's view on its data
  // source instead of its block.
  ActuationWorklogState* state = ActuationWorklogStateForBrowserActuationsBlock(
      BrowserActuationsBlock(block));
  // The SDK asks again on every layout change (e.g. collapsing, or each frame
  // while the response streams), so the view is only rebuilt when the block
  // shows different items.
  ActuationWorklogState* appliedState =
      [_appliedStates objectForKey:worklogView];
  if (!appliedState || !HaveSameItems(state, appliedState)) {
    [self applyState:state toView:worklogView];
  }
  return worklogView;
}

#pragma mark - Private

// Displays `state` in `view`.
- (void)applyState:(ActuationWorklogState*)state
            toView:(ActuationWorklogView*)view {
  // Set first so the trailing item's progress styling matches `state`.
  view.actuating = state.actuationActive;
  NSMutableArray<ActuationWorklogItem*>* items =
      [NSMutableArray arrayWithCapacity:state.entries.count];
  for (ActuationWorklogEntry* entry in state.entries) {
    [items addObject:entry.item];
  }
  [view setItems:items];
  [view setChip:state.entries.lastObject.chip];
  [_appliedStates setObject:state forKey:view];
}

@end
