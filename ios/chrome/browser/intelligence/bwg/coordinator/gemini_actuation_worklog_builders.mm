// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actuation_worklog_builders.h"

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data_builders.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_actuations_block.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size of the SF symbol of the task started item.
constexpr CGFloat kTaskStartedIconSize = 16.0;

// Returns an entry displaying `item`, without a chip.
ActuationWorklogEntry* EntryForItem(ActuationWorklogItem* item) {
  return [[ActuationWorklogEntry alloc] initWithItem:item chip:nil];
}

}  // namespace

ActuationWorklogItem* ActuationWorklogItemForTaskStarted() {
  return [ActuationWorklogItem
      labeledItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE)
                  subtitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_SUBTITLE)
                      icon:SymbolWithPointSize(SymbolPlayFill,
                                               kTaskStartedIconSize)];
}

ActuationWorklogState* ActuationWorklogStateForBrowserActuationsBlock(
    id<GeminiBrowserActuationsBlock> block) {
  NSMutableArray<ActuationWorklogEntry*>* entries = [NSMutableArray
      arrayWithObject:EntryForItem(ActuationWorklogItemForTaskStarted())];
  NSString* previousTitle = nil;
  for (id<GeminiBrowserActuation> actuation in block.actuations) {
    NSString* title = actuation.title;
    // Skips empty and consecutive duplicate titles, like the live store's
    // `appendTaskUpdate:`.
    if (title.length > 0 && ![title isEqualToString:previousTitle]) {
      [entries addObject:EntryForItem(
                             [ActuationWorklogItem simpleItemWithTitle:title])];
      previousTitle = title;
    }
    NSString* messageToUser = actuation.yieldAction.messageToUser;
    if (messageToUser.length > 0) {
      [entries addObject:EntryForItem([ActuationWorklogItem
                             freeformItemWithText:messageToUser])];
      // The message separates this action from the next one, so a repeated
      // title is a new step.
      previousTitle = nil;
    }
  }
  [entries addObject:EntryForItem(ActuationWorklogItemForTaskEnded())];

  // Completed tasks only display their entries, but a state requires a header.
  return [[ActuationWorklogState alloc]
      initWithActuationActive:NO
                headerContent:ActuationHeaderContentVariantsForTask(
                                  /*task_title=*/nil,
                                  actor::ActorTaskState::kFinished,
                                  /*current_step=*/nil)
                      entries:entries
                 intervention:nil
                  suggestions:nil
      selectedSuggestionIndex:NSNotFound
             suggestionsEntry:nil];
}
