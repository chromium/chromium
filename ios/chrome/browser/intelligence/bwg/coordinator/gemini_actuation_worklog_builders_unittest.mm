// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actuation_worklog_builders.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_actuations_block.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

// Actuation with settable fields.
@interface FakeGeminiBrowserActuation : NSObject <GeminiBrowserActuation>
@property(nonatomic, copy) NSString* title;
@property(nonatomic, strong) GeminiYieldAction* yieldAction;
@property(nonatomic, copy) NSString* userResponse;
@end

@implementation FakeGeminiBrowserActuation
@end

// Agent task block with settable actuations.
@interface FakeGeminiBrowserActuationsBlock
    : NSObject <GeminiBrowserActuationsBlock>
@property(nonatomic, copy) NSArray<id<GeminiBrowserActuation>>* actuations;
@end

@implementation FakeGeminiBrowserActuationsBlock
@end

namespace {

// Returns an actuation titled `title`, after which the agent sent
// `message_to_user` if it isn't nil.
FakeGeminiBrowserActuation* Actuation(NSString* title,
                                      NSString* message_to_user = nil) {
  FakeGeminiBrowserActuation* actuation =
      [[FakeGeminiBrowserActuation alloc] init];
  actuation.title = title;
  if (message_to_user) {
    actuation.yieldAction = [[GeminiYieldAction alloc]
        initWithReason:GeminiYieldReason::kConfirmation];
    actuation.yieldAction.messageToUser = message_to_user;
  }
  return actuation;
}

// Returns the worklog state of a completed task made of `actuations`.
ActuationWorklogState* StateForActuations(
    NSArray<id<GeminiBrowserActuation>>* actuations) {
  FakeGeminiBrowserActuationsBlock* block =
      [[FakeGeminiBrowserActuationsBlock alloc] init];
  block.actuations = actuations;
  return ActuationWorklogStateForBrowserActuationsBlock(block);
}

// Returns the items of the entries of `state`.
NSArray<ActuationWorklogItem*>* Items(ActuationWorklogState* state) {
  NSMutableArray<ActuationWorklogItem*>* items = [NSMutableArray array];
  for (ActuationWorklogEntry* entry in state.entries) {
    [items addObject:entry.item];
  }
  return items;
}

using GeminiActuationWorklogBuildersTest = PlatformTest;

// Tests that the task started item shows the task started title and subtitle.
TEST_F(GeminiActuationWorklogBuildersTest, TaskStartedItem) {
  ActuationWorklogItem* item = ActuationWorklogItemForTaskStarted();
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE),
              item.title);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_SUBTITLE),
      item.subtitle);
  EXPECT_EQ(ActuationWorklogItemStyle::kLabeled, item.style);
}

// Tests that a completed task without actuations only shows that it started
// and ended, without intervention, suggestions or suggestions entry.
TEST_F(GeminiActuationWorklogBuildersTest, TaskWithoutActuations) {
  ActuationWorklogState* state = StateForActuations(@[]);

  EXPECT_FALSE(state.actuationActive);
  EXPECT_EQ(nil, state.intervention);
  EXPECT_EQ(nil, state.suggestions);
  EXPECT_EQ(NSNotFound, state.selectedSuggestionIndex);
  EXPECT_EQ(nil, state.suggestionsEntry);
  NSArray<ActuationWorklogItem*>* items = Items(state);
  ASSERT_EQ(2u, items.count);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE),
              items[0].title);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_TASK_ENDED),
              items[1].title);
}

// Tests that each actuation adds a step, in order.
TEST_F(GeminiActuationWorklogBuildersTest, AddsStepPerActuation) {
  NSArray<ActuationWorklogItem*>* items = Items(StateForActuations(
      @[ Actuation(@"Search flights"), Actuation(@"Compare prices") ]));

  ASSERT_EQ(4u, items.count);
  EXPECT_NSEQ(@"Search flights", items[1].title);
  EXPECT_EQ(ActuationWorklogItemStyle::kSimple, items[1].style);
  EXPECT_NSEQ(@"Compare prices", items[2].title);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_TASK_ENDED),
              items[3].title);
}

// Tests that the agent's message to the user after an actuation follows its
// step.
TEST_F(GeminiActuationWorklogBuildersTest, AddsMessageAfterStep) {
  NSArray<ActuationWorklogItem*>* items = Items(StateForActuations(@[
    Actuation(@"Book flight", @"Buy now?"), Actuation(@"Confirm booking")
  ]));

  ASSERT_EQ(5u, items.count);
  EXPECT_NSEQ(@"Book flight", items[1].title);
  EXPECT_NSEQ(@"Buy now?", items[2].title);
  EXPECT_EQ(ActuationWorklogItemStyle::kFreeform, items[2].style);
  EXPECT_NSEQ(@"Confirm booking", items[3].title);
}

// Tests that empty titles and messages add no item.
TEST_F(GeminiActuationWorklogBuildersTest, SkipsEmptyTitlesAndMessages) {
  NSArray<ActuationWorklogItem*>* items = Items(StateForActuations(
      @[ Actuation(nil), Actuation(@"", @""), Actuation(@"Search flights") ]));

  ASSERT_EQ(3u, items.count);
  EXPECT_NSEQ(@"Search flights", items[1].title);
}

// Tests that consecutive actuations with the same title show a single step,
// while their messages are kept.
TEST_F(GeminiActuationWorklogBuildersTest, SkipsConsecutiveDuplicateTitles) {
  NSArray<ActuationWorklogItem*>* items = Items(StateForActuations(@[
    Actuation(@"Search flights"), Actuation(@"Search flights", @"Which date?"),
    Actuation(@"Compare prices"), Actuation(@"Search flights")
  ]));

  ASSERT_EQ(6u, items.count);
  EXPECT_NSEQ(@"Search flights", items[1].title);
  EXPECT_NSEQ(@"Which date?", items[2].title);
  EXPECT_NSEQ(@"Compare prices", items[3].title);
  EXPECT_NSEQ(@"Search flights", items[4].title);
}

// Tests that an agent message between two actuations with the same title keeps
// both steps.
TEST_F(GeminiActuationWorklogBuildersTest, ShowsRepeatedTitleAfterMessage) {
  NSArray<ActuationWorklogItem*>* items = Items(StateForActuations(
      @[ Actuation(@"Book flight", @"Buy now?"), Actuation(@"Book flight") ]));

  ASSERT_EQ(5u, items.count);
  EXPECT_NSEQ(@"Book flight", items[1].title);
  EXPECT_NSEQ(@"Buy now?", items[2].title);
  EXPECT_NSEQ(@"Book flight", items[3].title);
}

}  // namespace
