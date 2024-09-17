// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation_group.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/fake_tab_grid_transition_animation.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns an array of fake transition animations.
NSArray<FakeTransitionAnimation*>* CreateEmptyTabGridTransitionAnimations(
    int number_of_animations) {
  NSMutableArray<FakeTransitionAnimation*>* animations;
  for (int i = 0; i < number_of_animations; i++) {
    [animations addObject:[[FakeTransitionAnimation alloc] init]];
  }
  return animations;
}

}  // namespace

using TabGridTransitionAnimationGroupTest = PlatformTest;

// Checks that the completion block is executed when there is no animation.
TEST_F(TabGridTransitionAnimationGroupTest,
       testCompletionCalledWithoutAnimations) {
  // Create TabGridTransitionAnimationGroup with no animation.
  TabGridTransitionAnimationGroup* animation_group =
      [[TabGridTransitionAnimationGroup alloc] initWithAnimations:@[]];

  // Create expectation for completion block.
  __block BOOL completion_block_called = NO;
  ProceduralBlock completion_block = ^{
    completion_block_called = YES;
  };

  // Call `animateWithCompletion:`.
  [animation_group animateWithCompletion:completion_block];

  // Check that the completion block was called.
  EXPECT_TRUE(completion_block_called);
}

// Checks that all serial animations and the completion block have been
// executed.
TEST_F(TabGridTransitionAnimationGroupTest, testSerialAnimations) {
  // Create TabGridTransitionAnimationGroup with 5 animations.
  NSArray<id<TabGridTransitionAnimation>>* animations =
      CreateEmptyTabGridTransitionAnimations(5);
  TabGridTransitionAnimationGroup* animation_group =
      [[TabGridTransitionAnimationGroup alloc] initWithAnimations:animations];

  // Create expectation for completion block.
  __block BOOL completion_block_called = NO;
  ProceduralBlock completion_block = ^{
    completion_block_called = YES;
  };

  // Call `animateWithCompletion:`.
  [animation_group animateWithCompletion:completion_block];

  // Check that all animations and the completion block have been executed.
  EXPECT_TRUE(completion_block_called);
  for (FakeTransitionAnimation* animation in animations) {
    EXPECT_EQ(animation.animationCount, 1ul);
  }
}

// Checks that all concurrent animations and the completion block have been
// executed.
TEST_F(TabGridTransitionAnimationGroupTest, testConcurrentAnimations) {
  // Create TabGridTransitionAnimationGroup with 5 animations.
  NSArray<FakeTransitionAnimation*>* animations =
      CreateEmptyTabGridTransitionAnimations(5);
  TabGridTransitionAnimationGroup* animation_group =
      [[TabGridTransitionAnimationGroup alloc]
          initWithType:TabGridTransitionAnimationGroupType::kConcurrent
            animations:animations];

  // Create expectation for completion block.
  __block BOOL completion_block_called = NO;
  ProceduralBlock completion_block = ^{
    completion_block_called = YES;
  };

  // Call `animateWithCompletion:`.
  [animation_group animateWithCompletion:completion_block];

  // Check that all animations and the completion block have been executed.
  EXPECT_TRUE(completion_block_called);
  for (FakeTransitionAnimation* animation in animations) {
    EXPECT_EQ(animation.animationCount, 1ul);
  }
}
