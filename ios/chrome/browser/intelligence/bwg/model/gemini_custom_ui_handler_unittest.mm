// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_handler.h"

#import <UIKit/UIKit.h>

#import "base/test/gtest_util.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_block_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_view_provider.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// View provider that renders exactly one block and records the last request
// it received.
@interface FakeGeminiCustomUIViewProvider
    : NSObject <GeminiCustomUIViewProvider>
// The only block the provider renders.
@property(nonatomic, strong) id<GeminiBlockNode> providedBlock;
// The only block the provider hides, if any.
@property(nonatomic, strong) id<GeminiBlockNode> hiddenBlock;
@property(nonatomic, strong) UIView* viewToReturn;
@property(nonatomic, strong) id<GeminiBlockNode> lastBlock;
@property(nonatomic, strong) id<GeminiBlockContext> lastContext;
@property(nonatomic, strong) UIView* lastReusedView;
@property(nonatomic, assign) int callCount;
@end

@implementation FakeGeminiCustomUIViewProvider

- (BOOL)providesViewForBlock:(id<GeminiBlockNode>)block {
  return block == self.providedBlock;
}

- (BOOL)hidesBlock:(id<GeminiBlockNode>)block {
  return block == self.hiddenBlock;
}

- (UIView*)viewForBlock:(id<GeminiBlockNode>)block
                context:(id<GeminiBlockContext>)context
            reusingView:(UIView*)view {
  self.lastBlock = block;
  self.lastContext = context;
  self.lastReusedView = view;
  self.callCount++;
  return self.viewToReturn;
}

@end

namespace {

class GeminiCustomUIHandlerTest : public PlatformTest {
 protected:
  GeminiCustomUIHandlerTest() {
    handler_ = [[GeminiCustomUIHandler alloc] init];
    context_ = [[FakeGeminiBlockContext alloc] init];
    block_ = [[FakeGeminiBlockNode alloc] init];
    other_block_ = [[FakeGeminiBlockNode alloc] init];
    provider_ = MakeProviderForBlock(block_);
  }

  // Returns a provider that renders `block` only.
  FakeGeminiCustomUIViewProvider* MakeProviderForBlock(
      id<GeminiBlockNode> block) {
    FakeGeminiCustomUIViewProvider* provider =
        [[FakeGeminiCustomUIViewProvider alloc] init];
    provider.providedBlock = block;
    provider.viewToReturn = [[UIView alloc] init];
    return provider;
  }

  GeminiCustomUIHandler* handler_;
  FakeGeminiBlockContext* context_;
  FakeGeminiBlockNode* block_;
  FakeGeminiBlockNode* other_block_;
  // Renders `block_` only.
  FakeGeminiCustomUIViewProvider* provider_;
};

// Tests that no block is claimed or hidden while no provider is added.
TEST_F(GeminiCustomUIHandlerTest, ClaimsNothingWithoutProvider) {
  EXPECT_FALSE([handler_ providesCustomContentViewForBlockNode:block_]);
  EXPECT_FALSE([handler_ shouldHideBlockNode:block_]);
}

// Tests that only blocks some provider renders are claimed, and that rendered
// blocks are not hidden.
TEST_F(GeminiCustomUIHandlerTest, ClaimsOnlyProvidedBlocks) {
  [handler_ addViewProvider:provider_];

  EXPECT_TRUE([handler_ providesCustomContentViewForBlockNode:block_]);
  EXPECT_FALSE([handler_ shouldHideBlockNode:block_]);
  EXPECT_FALSE([handler_ providesCustomContentViewForBlockNode:other_block_]);
  EXPECT_FALSE([handler_ shouldHideBlockNode:other_block_]);
}

// Tests that a block is hidden iff some provider hides it.
TEST_F(GeminiCustomUIHandlerTest, HidesBlocksAProviderHides) {
  provider_.hiddenBlock = other_block_;
  [handler_ addViewProvider:provider_];

  EXPECT_TRUE([handler_ shouldHideBlockNode:other_block_]);
  EXPECT_FALSE([handler_ shouldHideBlockNode:block_]);

  [handler_ removeViewProvider:provider_];
  EXPECT_FALSE([handler_ shouldHideBlockNode:other_block_]);
}

// Tests that view requests are forwarded to the provider with the block,
// context and reusable view.
TEST_F(GeminiCustomUIHandlerTest, ForwardsViewRequestsToProvider) {
  [handler_ addViewProvider:provider_];

  UIView* view = [handler_ customContentViewForBlockNode:block_
                                                 context:context_
                                             reusingView:nil];
  EXPECT_EQ(provider_.viewToReturn, view);
  EXPECT_EQ(1, provider_.callCount);
  EXPECT_EQ(block_, provider_.lastBlock);
  EXPECT_EQ(context_, provider_.lastContext);
  EXPECT_EQ(nil, provider_.lastReusedView);

  view = [handler_ customContentViewForBlockNode:block_
                                         context:context_
                                     reusingView:view];
  EXPECT_EQ(provider_.viewToReturn, view);
  EXPECT_EQ(2, provider_.callCount);
  EXPECT_EQ(provider_.viewToReturn, provider_.lastReusedView);
}

// Tests that a view request for a block no provider renders still returns a
// view: the reusable view when there is one, a placeholder otherwise, and
// never reaches a provider for another block.
TEST_F(GeminiCustomUIHandlerTest, ReturnsViewForUnprovidedBlock) {
  [handler_ addViewProvider:provider_];

  UIView* reused = [[UIView alloc] init];
  EXPECT_EQ(reused, [handler_ customContentViewForBlockNode:other_block_
                                                    context:context_
                                                reusingView:reused]);
  EXPECT_NE(nil, [handler_ customContentViewForBlockNode:other_block_
                                                 context:context_
                                             reusingView:nil]);
  EXPECT_EQ(0, provider_.callCount);
}

// Tests that removing a provider stops claiming and forwarding, and that
// adding and removing repeatedly is harmless.
TEST_F(GeminiCustomUIHandlerTest, AddAndRemoveAreIdempotent) {
  [handler_ addViewProvider:provider_];
  [handler_ addViewProvider:provider_];
  EXPECT_TRUE([handler_ providesCustomContentViewForBlockNode:block_]);

  [handler_ removeViewProvider:provider_];
  [handler_ removeViewProvider:provider_];
  EXPECT_FALSE([handler_ providesCustomContentViewForBlockNode:block_]);
  EXPECT_FALSE([handler_ shouldHideBlockNode:block_]);

  UIView* reused = [[UIView alloc] init];
  EXPECT_EQ(reused, [handler_ customContentViewForBlockNode:block_
                                                    context:context_
                                                reusingView:reused]);
  EXPECT_EQ(0, provider_.callCount);

  [handler_ addViewProvider:provider_];
  EXPECT_TRUE([handler_ providesCustomContentViewForBlockNode:block_]);
}

// Tests that each provider only receives requests for the blocks it renders.
TEST_F(GeminiCustomUIHandlerTest, RoutesBlocksToTheirProvider) {
  FakeGeminiCustomUIViewProvider* other_provider =
      MakeProviderForBlock(other_block_);
  [handler_ addViewProvider:provider_];
  [handler_ addViewProvider:other_provider];

  EXPECT_EQ(other_provider.viewToReturn,
            [handler_ customContentViewForBlockNode:other_block_
                                            context:context_
                                        reusingView:nil]);
  EXPECT_EQ(provider_.viewToReturn,
            [handler_ customContentViewForBlockNode:block_
                                            context:context_
                                        reusingView:nil]);
  EXPECT_EQ(1, provider_.callCount);
  EXPECT_EQ(1, other_provider.callCount);
}

// Tests that two providers rendering the same block is a programming error.
TEST_F(GeminiCustomUIHandlerTest, DiesWhenProvidersOverlap) {
  FakeGeminiCustomUIViewProvider* overlapping = MakeProviderForBlock(block_);
  [handler_ addViewProvider:provider_];
  [handler_ addViewProvider:overlapping];

  EXPECT_CHECK_DEATH([handler_ providesCustomContentViewForBlockNode:block_]);
}

// Tests that the handler does not keep a deallocated provider alive or claim
// blocks for it.
TEST_F(GeminiCustomUIHandlerTest, HoldsProviderWeakly) {
  @autoreleasepool {
    FakeGeminiCustomUIViewProvider* provider = MakeProviderForBlock(block_);
    [handler_ addViewProvider:provider];
    EXPECT_TRUE([handler_ providesCustomContentViewForBlockNode:block_]);
  }
  EXPECT_FALSE([handler_ providesCustomContentViewForBlockNode:block_]);
}

}  // namespace
