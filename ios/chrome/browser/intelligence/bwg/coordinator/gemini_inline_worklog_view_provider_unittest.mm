// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_inline_worklog_view_provider.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actor_task_data_source.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_suggestions_presenter.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data_builders.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actuation_worklog_builders.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_block_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_actuations_block.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Actuation with a settable title.
@interface InlineWorklogFakeActuation : NSObject <GeminiBrowserActuation>
@property(nonatomic, copy) NSString* title;
@property(nonatomic, strong) GeminiYieldAction* yieldAction;
@property(nonatomic, copy) NSString* userResponse;
@end

@implementation InlineWorklogFakeActuation
@end

// Agent task with settable actuations.
@interface InlineWorklogFakeActuationsBlock
    : NSObject <GeminiBrowserActuationsBlock>
@property(nonatomic, copy) NSArray<id<GeminiBrowserActuation>>* actuations;
@end

@implementation InlineWorklogFakeActuationsBlock
@end

// Block node carrying a settable agent task.
@interface InlineWorklogFakeBlockNode
    : NSObject <GeminiBrowserActuationsBlockNode>
@property(nonatomic, strong) id<GeminiBrowserActuationsBlock>
    browserActuationsBlock;
@end

@implementation InlineWorklogFakeBlockNode
@end

// TODO(crbug.com/585107999): Replace with a shared fake in actor/test, also
// used by actuation_worklog_mediator_unittest.mm.
// Data source publishing settable states, which it replays to new observers.
@interface InlineWorklogFakeDataSource : NSObject <ActorTaskDataSource>
@property(nonatomic, strong, readonly) ActuationWorklogState* state;
// Number of current observers.
@property(nonatomic, readonly) NSUInteger observerCount;
// Sets `state` and sends it to the observers.
- (void)publishState:(ActuationWorklogState*)state;
@end

@implementation InlineWorklogFakeDataSource {
  NSHashTable<id<ActorTaskDataSourceObserver>>* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (NSUInteger)observerCount {
  return _observers.count;
}

- (void)publishState:(ActuationWorklogState*)state {
  _state = state;
  for (id<ActorTaskDataSourceObserver> observer in _observers.allObjects) {
    [observer taskDataSource:self didUpdateState:state];
  }
}

- (void)addObserver:(id<ActorTaskDataSourceObserver>)observer {
  [_observers addObject:observer];
  [observer taskDataSource:self didUpdateState:_state];
}

- (void)removeObserver:(id<ActorTaskDataSourceObserver>)observer {
  [_observers removeObject:observer];
}

@end

// Presenter recording the accessory it was asked to show the picker for.
@interface InlineWorklogFakeSuggestionsPresenter
    : NSObject <ActuationSuggestionsPresenter>
@property(nonatomic, strong, readonly)
    ActuationWorklogAccessoryItem* presentedAccessoryItem;
@end

@implementation InlineWorklogFakeSuggestionsPresenter

- (void)showSuggestionsPickerForAccessory:
    (ActuationWorklogAccessoryItem*)accessoryItem {
  _presentedAccessoryItem = accessoryItem;
}

@end

namespace {

using chrome_test_util::FindViewsByClass;

// Returns a block node carrying an agent task made of actuations titled
// `titles`.
InlineWorklogFakeBlockNode* BlockWithTitles(NSArray<NSString*>* titles) {
  NSMutableArray<id<GeminiBrowserActuation>>* actuations =
      [NSMutableArray array];
  for (NSString* title in titles) {
    InlineWorklogFakeActuation* actuation =
        [[InlineWorklogFakeActuation alloc] init];
    actuation.title = title;
    [actuations addObject:actuation];
  }
  InlineWorklogFakeActuationsBlock* block =
      [[InlineWorklogFakeActuationsBlock alloc] init];
  block.actuations = actuations;
  InlineWorklogFakeBlockNode* node = [[InlineWorklogFakeBlockNode alloc] init];
  node.browserActuationsBlock = block;
  return node;
}

// Returns the titles of the items displayed by `view`.
NSArray<NSString*>* DisplayedTitles(UIView* view) {
  NSMutableArray<NSString*>* titles = [NSMutableArray array];
  for (ActuationWorklogItemView* item_view in
           FindViewsByClass<ActuationWorklogItemView>(view)) {
    [titles addObject:item_view.item.title];
  }
  return titles;
}

// Returns the titles a view built from `block` displays.
NSArray<NSString*>* BlockTitles(InlineWorklogFakeBlockNode* block) {
  NSMutableArray<NSString*>* titles = [NSMutableArray array];
  for (ActuationWorklogEntry* entry in
           ActuationWorklogStateForBrowserActuationsBlock(
               block.browserActuationsBlock)
               .entries) {
    [titles addObject:entry.item.title];
  }
  return titles;
}

// Returns an entry displaying a simple item titled `title`.
ActuationWorklogEntry* Entry(NSString* title) {
  return [[ActuationWorklogEntry alloc]
      initWithItem:[ActuationWorklogItem simpleItemWithTitle:title]
              chip:nil];
}

// Returns the state of a running task displaying `entries`.
ActuationWorklogState* RunningState(NSArray<ActuationWorklogEntry*>* entries) {
  return [[ActuationWorklogState alloc]
      initWithActuationActive:YES
                headerContent:ActuationHeaderContentVariantsForTask(
                                  /*task_title=*/nil,
                                  actor::ActorTaskState::kActing,
                                  /*current_step=*/nil)
                      entries:entries
                 intervention:nil
                  suggestions:nil
      selectedSuggestionIndex:NSNotFound
             suggestionsEntry:nil];
}

// Returns the context of the latest response while it streams, i.e. the one
// running a task.
FakeGeminiBlockContext* StreamingContext() {
  FakeGeminiBlockContext* context = [[FakeGeminiBlockContext alloc] init];
  context.responseState = GeminiResponseState::kStreaming;
  return context;
}

class GeminiInlineWorklogViewProviderTest : public PlatformTest {
 protected:
  // Returns the view vended for `block` in `context` (a completed latest
  // response if nil), reusing `view` if not nil.
  UIView* ViewForBlock(id<GeminiBlockNode> block,
                       UIView* view = nil,
                       id<GeminiBlockContext> context = nil) {
    return
        [provider_ viewForBlock:block
                        context:context ?: [[FakeGeminiBlockContext alloc] init]
                    reusingView:view];
  }

  InlineWorklogFakeSuggestionsPresenter* suggestions_presenter_ =
      [[InlineWorklogFakeSuggestionsPresenter alloc] init];
  GeminiInlineWorklogViewProvider* provider_ =
      [[GeminiInlineWorklogViewProvider alloc]
          initWithSuggestionsPresenter:suggestions_presenter_];
  InlineWorklogFakeDataSource* data_source_ =
      [[InlineWorklogFakeDataSource alloc] init];
};

// Tests that only blocks carrying an agent task are rendered, and that no
// block is hidden.
TEST_F(GeminiInlineWorklogViewProviderTest, ProvidesOnlyAgentTaskBlocks) {
  InlineWorklogFakeBlockNode* agent_task_block = BlockWithTitles(@[]);
  InlineWorklogFakeBlockNode* empty_node =
      [[InlineWorklogFakeBlockNode alloc] init];
  FakeGeminiBlockNode* other_block = [[FakeGeminiBlockNode alloc] init];

  EXPECT_TRUE([provider_ providesViewForBlock:agent_task_block]);
  EXPECT_FALSE([provider_ providesViewForBlock:empty_node]);
  EXPECT_FALSE([provider_ providesViewForBlock:other_block]);
  EXPECT_FALSE([provider_ hidesBlock:agent_task_block]);
  EXPECT_FALSE([provider_ hidesBlock:other_block]);
}

// Tests that a past task's view shows the block's actions, collapsed and
// clipped.
TEST_F(GeminiInlineWorklogViewProviderTest, ShowsPastTaskCollapsed) {
  UIView* view = ViewForBlock(BlockWithTitles(@[ @"Search flights" ]));

  ActuationWorklogView* worklog_view =
      base::apple::ObjCCast<ActuationWorklogView>(view);
  ASSERT_TRUE(worklog_view);
  EXPECT_TRUE(worklog_view.collapsed);
  EXPECT_FALSE(worklog_view.actuating);
  EXPECT_TRUE(worklog_view.clipsToBounds);
  EXPECT_TRUE([DisplayedTitles(view) containsObject:@"Search flights"]);
}

// Tests that asking again with an unchanged block keeps the displayed items,
// while a block with new actions updates them.
TEST_F(GeminiInlineWorklogViewProviderTest, RebuildsOnlyChangedBlocks) {
  UIView* view = ViewForBlock(BlockWithTitles(@[ @"Search flights" ]));
  NSArray<ActuationWorklogItemView*>* item_views =
      FindViewsByClass<ActuationWorklogItemView>(view);

  EXPECT_EQ(view, ViewForBlock(BlockWithTitles(@[ @"Search flights" ]), view));
  EXPECT_NSEQ(item_views, FindViewsByClass<ActuationWorklogItemView>(view));

  EXPECT_EQ(
      view,
      ViewForBlock(BlockWithTitles(@[ @"Search flights", @"Compare prices" ]),
                   view));
  EXPECT_TRUE([DisplayedTitles(view) containsObject:@"Compare prices"]);
}

// Tests that the running task's view follows its data source, expanded.
TEST_F(GeminiInlineWorklogViewProviderTest, BindsRunningTaskView) {
  [data_source_ publishState:RunningState(@[ Entry(@"Live step") ])];
  [provider_ startObservingDataSource:data_source_];

  UIView* view = ViewForBlock(BlockWithTitles(@[ @"Block step" ]), nil,
                              StreamingContext());

  ActuationWorklogView* worklog_view =
      base::apple::ObjCCastStrict<ActuationWorklogView>(view);
  EXPECT_NSEQ(@[ @"Live step" ], DisplayedTitles(view));
  EXPECT_FALSE(worklog_view.collapsed);
  EXPECT_TRUE(worklog_view.actuating);
  EXPECT_EQ(1u, data_source_.observerCount);
}

// Tests that views of other responses keep showing their block while a task
// runs.
TEST_F(GeminiInlineWorklogViewProviderTest, DoesNotBindOtherResponses) {
  [data_source_ publishState:RunningState(@[ Entry(@"Live step") ])];
  [provider_ startObservingDataSource:data_source_];
  FakeGeminiBlockContext* older_response = StreamingContext();
  older_response.latestResponse = NO;

  InlineWorklogFakeBlockNode* older_block = BlockWithTitles(@[ @"Old step" ]);
  InlineWorklogFakeBlockNode* done_block = BlockWithTitles(@[ @"Done step" ]);
  UIView* older_view = ViewForBlock(older_block, nil, older_response);
  UIView* done_view = ViewForBlock(done_block);

  EXPECT_NSEQ(BlockTitles(older_block), DisplayedTitles(older_view));
  EXPECT_NSEQ(BlockTitles(done_block), DisplayedTitles(done_view));
  EXPECT_TRUE(
      base::apple::ObjCCastStrict<ActuationWorklogView>(older_view).collapsed);
  EXPECT_EQ(0u, data_source_.observerCount);
}

// Tests that the running task's view stays empty until its data source
// arrives, then follows it.
TEST_F(GeminiInlineWorklogViewProviderTest, WaitingViewGoesLive) {
  UIView* view = ViewForBlock(BlockWithTitles(@[ @"Block step" ]), nil,
                              StreamingContext());
  EXPECT_EQ(0u, DisplayedTitles(view).count);

  [data_source_ publishState:RunningState(@[ Entry(@"Live step") ])];
  [provider_ startObservingDataSource:data_source_];

  EXPECT_NSEQ(@[ @"Live step" ], DisplayedTitles(view));
  EXPECT_FALSE(
      base::apple::ObjCCastStrict<ActuationWorklogView>(view).collapsed);
}

// Tests that a reused view that showed a block follows the running task,
// expanded, once it is vended for it.
TEST_F(GeminiInlineWorklogViewProviderTest, BindsReusedBlockView) {
  UIView* view = ViewForBlock(BlockWithTitles(@[ @"Old step" ]));
  [data_source_ publishState:RunningState(@[ Entry(@"Live step") ])];
  [provider_ startObservingDataSource:data_source_];

  ViewForBlock(BlockWithTitles(@[]), view, StreamingContext());

  EXPECT_NSEQ(@[ @"Live step" ], DisplayedTitles(view));
  EXPECT_FALSE(
      base::apple::ObjCCastStrict<ActuationWorklogView>(view).collapsed);
}

// Tests that a streaming response whose task never reports a data source shows
// its block once done.
TEST_F(GeminiInlineWorklogViewProviderTest, ShowsBlockWithoutDataSource) {
  InlineWorklogFakeBlockNode* block = BlockWithTitles(@[ @"Block step" ]);
  UIView* view = ViewForBlock(block, nil, StreamingContext());
  EXPECT_EQ(0u, DisplayedTitles(view).count);

  ViewForBlock(block, view);

  EXPECT_NSEQ(BlockTitles(block), DisplayedTitles(view));
}

// Tests that live updates replace the last item in place and append the new
// ones, keeping the other item views.
TEST_F(GeminiInlineWorklogViewProviderTest, AppliesLiveUpdatesIncrementally) {
  ActuationWorklogEntry* first = Entry(@"First");
  [data_source_ publishState:RunningState(@[ first, Entry(@"Second") ])];
  [provider_ startObservingDataSource:data_source_];
  UIView* view = ViewForBlock(BlockWithTitles(@[]), nil, StreamingContext());
  NSArray<ActuationWorklogItemView*>* item_views =
      FindViewsByClass<ActuationWorklogItemView>(view);
  ASSERT_EQ(2u, item_views.count);

  [data_source_
      publishState:RunningState(
                       @[ first, Entry(@"Updated"), Entry(@"Third") ])];

  NSArray<ActuationWorklogItemView*>* updated_item_views =
      FindViewsByClass<ActuationWorklogItemView>(view);
  ASSERT_EQ(3u, updated_item_views.count);
  EXPECT_EQ(item_views[0], updated_item_views[0]);
  EXPECT_EQ(item_views[1], updated_item_views[1]);
  EXPECT_NSEQ((@[ @"First", @"Updated", @"Third" ]), DisplayedTitles(view));
}

// Tests that a changed entry before the last one rebuilds the running task's
// view.
TEST_F(GeminiInlineWorklogViewProviderTest, RebuildsOnEarlierLiveChange) {
  ActuationWorklogEntry* second = Entry(@"Second");
  [data_source_ publishState:RunningState(@[ Entry(@"First"), second ])];
  [provider_ startObservingDataSource:data_source_];
  UIView* view = ViewForBlock(BlockWithTitles(@[]), nil, StreamingContext());
  NSArray<ActuationWorklogItemView*>* item_views =
      FindViewsByClass<ActuationWorklogItemView>(view);
  ASSERT_EQ(2u, item_views.count);

  [data_source_ publishState:RunningState(@[ Entry(@"New first"), second ])];

  NSArray<ActuationWorklogItemView*>* updated_item_views =
      FindViewsByClass<ActuationWorklogItemView>(view);
  ASSERT_EQ(2u, updated_item_views.count);
  EXPECT_NE(item_views[0], updated_item_views[0]);
  EXPECT_NSEQ((@[ @"New first", @"Second" ]), DisplayedTitles(view));
}

// Tests that the running task's view keeps its live content once the task
// stops, even when its block changes, and no longer looks in progress.
TEST_F(GeminiInlineWorklogViewProviderTest, KeepsLiveContentAfterStop) {
  [data_source_ publishState:RunningState(@[ Entry(@"Live step") ])];
  [provider_ startObservingDataSource:data_source_];
  UIView* view = ViewForBlock(BlockWithTitles(@[]), nil, StreamingContext());

  [provider_ stopObservingDataSource];
  ViewForBlock(BlockWithTitles(@[ @"Block step" ]), view);

  ActuationWorklogView* worklog_view =
      base::apple::ObjCCastStrict<ActuationWorklogView>(view);
  EXPECT_NSEQ(@[ @"Live step" ], DisplayedTitles(view));
  EXPECT_FALSE(worklog_view.collapsed);
  EXPECT_FALSE(worklog_view.actuating);
  EXPECT_EQ(0u, data_source_.observerCount);
}

// Tests that the running task's view shows its block once done, collapsed like
// other past worklogs, if the task stopped without publishing any state.
TEST_F(GeminiInlineWorklogViewProviderTest, ShowsBlockAfterStopWithoutState) {
  [provider_ startObservingDataSource:data_source_];
  InlineWorklogFakeBlockNode* block = BlockWithTitles(@[ @"Block step" ]);
  UIView* view = ViewForBlock(block, nil, StreamingContext());
  EXPECT_EQ(0u, DisplayedTitles(view).count);

  [provider_ stopObservingDataSource];
  ViewForBlock(block, view);

  EXPECT_NSEQ(BlockTitles(block), DisplayedTitles(view));
  EXPECT_TRUE(
      base::apple::ObjCCastStrict<ActuationWorklogView>(view).collapsed);
}

// Tests that a new task's data source replaces the previous one, whose view no
// longer updates.
TEST_F(GeminiInlineWorklogViewProviderTest, NewTaskReplacesBinding) {
  [data_source_ publishState:RunningState(@[ Entry(@"First task") ])];
  [provider_ startObservingDataSource:data_source_];
  UIView* first_view =
      ViewForBlock(BlockWithTitles(@[]), nil, StreamingContext());

  InlineWorklogFakeDataSource* second_data_source =
      [[InlineWorklogFakeDataSource alloc] init];
  [second_data_source publishState:RunningState(@[ Entry(@"Second task") ])];
  [provider_ startObservingDataSource:second_data_source];
  // A late update from the previous task's data source is ignored.
  [static_cast<id<ActorTaskDataSourceObserver>>(provider_)
      taskDataSource:data_source_
      didUpdateState:RunningState(@[ Entry(@"Ignored") ])];
  UIView* second_view =
      ViewForBlock(BlockWithTitles(@[]), nil, StreamingContext());

  EXPECT_EQ(0u, data_source_.observerCount);
  EXPECT_NSEQ(@[ @"First task" ], DisplayedTitles(first_view));
  EXPECT_NSEQ(@[ @"Second task" ], DisplayedTitles(second_view));
}

// Tests that the data source's nil state, before its task registers, is
// ignored until a real state is published.
TEST_F(GeminiInlineWorklogViewProviderTest, IgnoresNilState) {
  [provider_ startObservingDataSource:data_source_];
  UIView* view = ViewForBlock(BlockWithTitles(@[]), nil, StreamingContext());
  EXPECT_EQ(0u, DisplayedTitles(view).count);

  [data_source_ publishState:RunningState(@[ Entry(@"Live step") ])];

  EXPECT_NSEQ(@[ @"Live step" ], DisplayedTitles(view));
}

// Tests that tapping a suggestion card in a vended view, including a reused
// one, asks the presenter to show its picker.
TEST_F(GeminiInlineWorklogViewProviderTest, ForwardsAccessoryTapToPresenter) {
  ActuationWorklogAccessoryItem* accessory_item =
      [[ActuationWorklogAccessoryItem alloc] initWithIcon:nil
                                                    title:@"user@example.com"
                                                 subtitle:nil
                                               detailText:nil
                                               hasChevron:NO];
  ActuationWorklogView* reused_view =
      [[ActuationWorklogView alloc] initWithFrame:CGRectZero];
  ActuationWorklogView* worklog_view =
      base::apple::ObjCCastStrict<ActuationWorklogView>(
          ViewForBlock(BlockWithTitles(@[]), reused_view));
  ASSERT_EQ(reused_view, worklog_view);

  [worklog_view.delegate worklogView:worklog_view
                 didTapAccessoryItem:accessory_item];

  EXPECT_EQ(accessory_item, suggestions_presenter_.presentedAccessoryItem);
}

}  // namespace
