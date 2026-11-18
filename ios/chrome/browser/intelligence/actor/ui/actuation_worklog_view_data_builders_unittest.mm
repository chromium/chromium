// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data_builders.h"

#import <utility>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kTaskTitle = @"Task Title";
NSString* const kCurrentStep = @"Clicking button";

using enum actor::ActorTaskState;
using enum actor::ToolType;

using ActuationWorklogViewDataBuildersTest = PlatformTest;

// Tests that tools without a dedicated chip get a fallback chip.
TEST_F(ActuationWorklogViewDataBuildersTest, TestFallbackChipForUnmappedTool) {
  EXPECT_TRUE(ActuationWorklogChipForToolType(kSelect));
}

// Tests that no chip is built without a tool or for tools without a chip.
TEST_F(ActuationWorklogViewDataBuildersTest, TestNoChipForToolType) {
  EXPECT_FALSE(ActuationWorklogChipForToolType(std::nullopt));
  EXPECT_FALSE(ActuationWorklogChipForToolType(kWaitZeroDuration));
  EXPECT_FALSE(ActuationWorklogChipForToolType(kUnknown));
}

// Tests that the header's primary action is pause for a pausable task, and stop
// for any other task.
TEST_F(ActuationWorklogViewDataBuildersTest,
       TestHeaderPrimaryActionForTaskState) {
  const std::pair<actor::ActorTaskState, ActuationHeaderPrimaryAction>
      kExpectations[] = {
          {kInit, ActuationHeaderPrimaryAction::kPause},
          {kActing, ActuationHeaderPrimaryAction::kPause},
          {kReflecting, ActuationHeaderPrimaryAction::kPause},
          {kWaitingOnUser, ActuationHeaderPrimaryAction::kPause},
          {kPausedByActor, ActuationHeaderPrimaryAction::kStop},
          {kPausedByUser, ActuationHeaderPrimaryAction::kStop},
          {kCancelled, ActuationHeaderPrimaryAction::kStop},
          {kFinished, ActuationHeaderPrimaryAction::kStop},
          {kFailed, ActuationHeaderPrimaryAction::kStop},
      };
  for (const auto& [state, action] : kExpectations) {
    SCOPED_TRACE(static_cast<int>(state));
    EXPECT_EQ(ActuationHeaderPrimaryActionForTaskState(state), action);
  }
}

// Tests that while the task runs, including while paused, the minimized header
// shows the task title above the current step, and the other modes only show
// the task title.
TEST_F(ActuationWorklogViewDataBuildersTest, TestHeaderContentWhileRunning) {
  for (actor::ActorTaskState state :
       {kInit, kActing, kReflecting, kPausedByActor, kPausedByUser}) {
    SCOPED_TRACE(static_cast<int>(state));
    ActuationHeaderContentVariants* variants =
        ActuationHeaderContentVariantsForTask(kTaskTitle, state, kCurrentStep);

    ActuationHeaderContent* minimized =
        [variants contentForDisplayMode:ActuationWorklogDisplayModeMinimized];
    EXPECT_NSEQ(minimized.title, kTaskTitle);
    EXPECT_NSEQ(minimized.subtitle, kCurrentStep);

    for (ActuationWorklogDisplayMode mode :
         {ActuationWorklogDisplayModeCompact,
          ActuationWorklogDisplayModeExpanded}) {
      SCOPED_TRACE(mode);
      ActuationHeaderContent* content = [variants contentForDisplayMode:mode];
      EXPECT_NSEQ(content.title, kTaskTitle);
      EXPECT_FALSE(content.subtitle);
    }
  }
}

// Tests that the minimized header shows the status above the task title for
// states with a status.
TEST_F(ActuationWorklogViewDataBuildersTest, TestHeaderContentShowsStatus) {
  const std::pair<actor::ActorTaskState, int> kStatuses[] = {
      {kWaitingOnUser, IDS_IOS_ACTOR_HEADER_INPUT_NEEDED},
      {kFinished, IDS_IOS_ACTOR_HEADER_TASK_DONE},
      {kCancelled, IDS_IOS_ACTOR_HEADER_TASK_STOPPED},
      {kFailed, IDS_IOS_ACTOR_HEADER_TASK_FAILED},
  };
  for (const auto& [state, message_id] : kStatuses) {
    SCOPED_TRACE(static_cast<int>(state));
    ActuationHeaderContentVariants* variants =
        ActuationHeaderContentVariantsForTask(kTaskTitle, state, kCurrentStep);

    ActuationHeaderContent* minimized =
        [variants contentForDisplayMode:ActuationWorklogDisplayModeMinimized];
    EXPECT_NSEQ(minimized.title, l10n_util::GetNSString(message_id));
    EXPECT_NSEQ(minimized.subtitle, kTaskTitle);

    for (ActuationWorklogDisplayMode mode :
         {ActuationWorklogDisplayModeCompact,
          ActuationWorklogDisplayModeExpanded}) {
      SCOPED_TRACE(mode);
      ActuationHeaderContent* content = [variants contentForDisplayMode:mode];
      EXPECT_NSEQ(content.title, kTaskTitle);
      EXPECT_FALSE(content.subtitle);
    }
  }
}

// Tests that the user response item quotes the response and is active.
TEST_F(ActuationWorklogViewDataBuildersTest, TestItemForUserResponse) {
  ActuationWorklogItem* item = ActuationWorklogItemForUserResponse(@"Allow");

  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_ACTOR_WORKLOG_USER_RESPONDED_TITLE));
  EXPECT_NSEQ(
      item.subtitle,
      l10n_util::GetNSStringF(IDS_IOS_ACTOR_WORKLOG_USER_RESPONSE, u"Allow"));
  EXPECT_TRUE(item.icon);
  EXPECT_TRUE(item.active);
}

// Tests that the task continues item is active.
TEST_F(ActuationWorklogViewDataBuildersTest, TestItemForTaskContinues) {
  ActuationWorklogItem* item = ActuationWorklogItemForTaskContinues();

  EXPECT_NSEQ(item.title,
              l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_TASK_CONTINUES));
  EXPECT_FALSE(item.subtitle);
  EXPECT_TRUE(item.icon);
  EXPECT_TRUE(item.active);
}

// Tests that the task ended item is inactive, so that it isn't kept visible
// as a running step when the worklog is collapsed.
TEST_F(ActuationWorklogViewDataBuildersTest, TestItemForTaskEnded) {
  ActuationWorklogItem* item = ActuationWorklogItemForTaskEnded();

  EXPECT_NSEQ(item.title,
              l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_TASK_ENDED));
  EXPECT_FALSE(item.subtitle);
  EXPECT_TRUE(item.icon);
  EXPECT_FALSE(item.active);
}

}  // namespace
