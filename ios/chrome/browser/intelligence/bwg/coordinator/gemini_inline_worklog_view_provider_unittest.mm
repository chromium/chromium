// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_inline_worklog_view_provider.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
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

class GeminiInlineWorklogViewProviderTest : public PlatformTest {
 protected:
  // Returns the view vended for `block`, reusing `view` if not nil.
  UIView* ViewForBlock(id<GeminiBlockNode> block, UIView* view = nil) {
    return [provider_ viewForBlock:block
                           context:[[FakeGeminiBlockContext alloc] init]
                       reusingView:view];
  }

  GeminiInlineWorklogViewProvider* provider_ =
      [[GeminiInlineWorklogViewProvider alloc] init];
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

}  // namespace
