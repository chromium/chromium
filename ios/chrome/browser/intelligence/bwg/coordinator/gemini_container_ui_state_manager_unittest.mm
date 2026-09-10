// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_ui_state_manager.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface GeminiContainerUIStateManager (Testing)
@property(nonatomic, readonly) ios::provider::GeminiClientMode processingStatus;
@property(nonatomic, readonly) ios::provider::GeminiViewMode viewMode;
@property(nonatomic, assign) BOOL hasConversation;
@property(nonatomic, assign) GeminiContainerUIState currentUIState;
@end

@interface FakeGeminiContainerUIStateManagerDelegate
    : NSObject <GeminiContainerUIStateManagerDelegate>
@property(nonatomic, assign) GeminiContainerUIState lastUIState;
@property(nonatomic, assign) NSInteger changeCount;
- (void)reset;
@end

@implementation FakeGeminiContainerUIStateManagerDelegate

- (void)didChangeUIState:(GeminiContainerUIState)containerUIState {
  _lastUIState = containerUIState;
  _changeCount++;
}

- (void)reset {
  _lastUIState = {};
  _changeCount = 0;
}

@end

class GeminiContainerUIStateManagerTest : public PlatformTest {
 public:
  GeminiContainerUIStateManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        state_manager_([[GeminiContainerUIStateManager alloc] init]),
        delegate_([[FakeGeminiContainerUIStateManagerDelegate alloc] init]) {
    state_manager_.delegate = delegate_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  GeminiContainerUIStateManager* state_manager_;
  FakeGeminiContainerUIStateManagerDelegate* delegate_;
};

// Tests that GeminiContainerUIState factory methods produce the expected
// configurations.
TEST_F(GeminiContainerUIStateManagerTest, TestUIStateFactoryMethods) {
  GeminiContainerUIState default_zero_state =
      GeminiContainerUIState::ZeroState();
  EXPECT_EQ(AssistantContainerDetent::kMedium, default_zero_state.detent);
  EXPECT_TRUE(default_zero_state.hasGrabber);
  EXPECT_TRUE(default_zero_state.zeroStateVisible);

  GeminiContainerUIState custom_zero_state =
      GeminiContainerUIState::ZeroState(AssistantContainerDetent::kLarge);
  EXPECT_EQ(AssistantContainerDetent::kLarge, custom_zero_state.detent);
  EXPECT_TRUE(custom_zero_state.hasGrabber);
  EXPECT_TRUE(custom_zero_state.zeroStateVisible);

  GeminiContainerUIState expanded = GeminiContainerUIState::ExpandedResponse();
  EXPECT_EQ(AssistantContainerDetent::kMedium, expanded.detent);
  EXPECT_TRUE(expanded.hasGrabber);
  EXPECT_FALSE(expanded.zeroStateVisible);

  GeminiContainerUIState minimized_without_grabber =
      GeminiContainerUIState::Minimized(/*has_grabber=*/NO);
  EXPECT_EQ(AssistantContainerDetent::kMinimized,
            minimized_without_grabber.detent);
  EXPECT_FALSE(minimized_without_grabber.hasGrabber);
  EXPECT_FALSE(minimized_without_grabber.zeroStateVisible);

  GeminiContainerUIState minimized_with_grabber =
      GeminiContainerUIState::Minimized(/*has_grabber=*/YES);
  EXPECT_EQ(AssistantContainerDetent::kMinimized,
            minimized_with_grabber.detent);
  EXPECT_TRUE(minimized_with_grabber.hasGrabber);
  EXPECT_FALSE(minimized_with_grabber.zeroStateVisible);
}

// Tests that initial state properties are correctly set upon initialization.
TEST_F(GeminiContainerUIStateManagerTest, TestInitialProperties) {
  EXPECT_EQ(ios::provider::GeminiViewMode::kUnknown, state_manager_.viewMode);
  EXPECT_EQ(ios::provider::GeminiClientMode::kUnknown,
            state_manager_.processingStatus);
  EXPECT_EQ(AssistantContainerDetent::kMinimized,
            state_manager_.currentUIState.detent);
  EXPECT_FALSE(state_manager_.currentUIState.hasGrabber);
  EXPECT_FALSE(state_manager_.currentUIState.zeroStateVisible);
  EXPECT_FALSE(state_manager_.hasConversation);
}

// Tests that setupInitialUIState returns the default zero state configuration.
TEST_F(GeminiContainerUIStateManagerTest, TestInitialState) {
  [state_manager_ setupInitialUIState];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_TRUE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_FALSE(state_manager_.hasConversation);
  EXPECT_EQ(ios::provider::GeminiViewMode::kFloaty, state_manager_.viewMode);
  EXPECT_EQ(ios::provider::GeminiClientMode::kDormant,
            state_manager_.processingStatus);
}

// Tests that switching to Live mode configures a minimized container without a
// grabber.
TEST_F(GeminiContainerUIStateManagerTest, TestSwitchToLiveMode) {
  [state_manager_ setupInitialUIState];
  [delegate_ reset];
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kLive];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_FALSE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_FALSE(state_manager_.hasConversation);
}

// Tests that switching to kLive minimizes the container, hides the grabber,
// and removes the zero state while preserving conversation state, and
// subsequent processing status changes in live mode do not override live mode
// container dimensions while transitioning conversation state to true on
// response.
TEST_F(GeminiContainerUIStateManagerTest, TestUpdateUIStateFromLiveMode) {
  [state_manager_ setupInitialUIState];
  EXPECT_FALSE(state_manager_.hasConversation);

  [delegate_ reset];
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kLive];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_FALSE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_FALSE(state_manager_.hasConversation);

  // Subsequent processing status changes should not notify delegate in live
  // mode, but responding changes conversation state to YES.
  [delegate_ reset];
  [state_manager_ transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                                   kResponding];
  EXPECT_EQ(0, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized,
            state_manager_.currentUIState.detent);
  EXPECT_FALSE(state_manager_.currentUIState.hasGrabber);
  EXPECT_FALSE(state_manager_.currentUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);
}

// Tests that switching from live to floaty expands the container to medium,
// adds a grabber, and sets zero state when conversation state was preserved.
TEST_F(GeminiContainerUIStateManagerTest,
       TestSwitchFromLiveToFloatyWithPreservedZeroState) {
  [state_manager_ setupInitialUIState];
  EXPECT_FALSE(state_manager_.hasConversation);

  // Switch to live mode.
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kLive];
  EXPECT_FALSE(state_manager_.hasConversation);

  // Switch back to floaty without response. Grabber is added, container
  // expands, and preserved zero state is restored.
  [delegate_ reset];
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kFloaty];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_TRUE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_FALSE(state_manager_.hasConversation);
}

// Tests that switching from live to floaty adds a grabber, expands the
// container to medium, and sets zero state to NO when a response occurred
// during live mode.
TEST_F(GeminiContainerUIStateManagerTest,
       TestSwitchFromLiveToFloatyAfterResponse) {
  [state_manager_ setupInitialUIState];
  EXPECT_FALSE(state_manager_.hasConversation);

  // Switch to live mode.
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kLive];

  // Gemini responds in live mode.
  [state_manager_ transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                                   kResponding];
  EXPECT_TRUE(state_manager_.hasConversation);

  // Switch back to floaty mode. Grabber is added, container expands, and zero
  // state NO is returned.
  [delegate_ reset];
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kFloaty];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);
}

// Tests that switching back to Floaty mode with an active conversation returns
// expanded response.
TEST_F(GeminiContainerUIStateManagerTest, TestSwitchToFloatyWithConversation) {
  [state_manager_ setupInitialUIState];
  state_manager_.hasConversation = YES;
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kLive];

  [delegate_ reset];
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kFloaty];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);
}

// Tests that transitionToMode with kFloaty does not change the default
// container UI state values when already in Floaty mode.
TEST_F(GeminiContainerUIStateManagerTest,
       TestTransitionToModeFloatyPreservesDefaultUIState) {
  [state_manager_ setupInitialUIState];

  EXPECT_EQ(AssistantContainerDetent::kMedium,
            state_manager_.currentUIState.detent);
  EXPECT_TRUE(state_manager_.currentUIState.hasGrabber);
  EXPECT_TRUE(state_manager_.currentUIState.zeroStateVisible);
  EXPECT_FALSE(state_manager_.hasConversation);

  [delegate_ reset];
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kFloaty];
  EXPECT_EQ(0, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium,
            state_manager_.currentUIState.detent);
  EXPECT_TRUE(state_manager_.currentUIState.hasGrabber);
  EXPECT_TRUE(state_manager_.currentUIState.zeroStateVisible);
  EXPECT_FALSE(state_manager_.hasConversation);
}

// Tests that updating processing status correctly transitions state based on
// processingStatus.
TEST_F(GeminiContainerUIStateManagerTest,
       TestUpdateUIStateFromProcessingStatus) {
  [state_manager_ setupInitialUIState];

  [delegate_ reset];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kThinking];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_FALSE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);

  [delegate_ reset];
  [state_manager_ transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                                   kResponding];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);

  [delegate_ reset];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                       kPreviousConversationLoading];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);
}

// Tests that in floaty mode, thinking and previous conversation loading both
// set hasConversation to YES and zero state to NO.
TEST_F(GeminiContainerUIStateManagerTest,
       TestFloatyModeThinkingAndPreviousConversationLoadingSetsZeroStateToNo) {
  [state_manager_ setupInitialUIState];
  EXPECT_FALSE(state_manager_.hasConversation);

  // Transition to thinking.
  [delegate_ reset];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kThinking];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_TRUE(state_manager_.hasConversation);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);

  // Reset conversation to NO via new chat button.
  [delegate_ reset];
  [state_manager_ handleNewChat];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_FALSE(state_manager_.hasConversation);
  EXPECT_TRUE(delegate_.lastUIState.zeroStateVisible);

  // Transition to previous conversation loading.
  [delegate_ reset];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                       kPreviousConversationLoading];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_TRUE(state_manager_.hasConversation);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
}

// Tests that Responding status arriving within the allowed interval
// auto-expands the container.
TEST_F(GeminiContainerUIStateManagerTest, TestRespondingWithinIntervalExpands) {
  [state_manager_ setupInitialUIState];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kThinking];

  task_environment_.FastForwardBy(
      base::Seconds(GetGeminiResponseReadyInterval() - 1));

  [delegate_ reset];
  [state_manager_ transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                                   kResponding];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);
}

// Tests that Responding status arriving after the allowed interval keeps the
// container minimized with a grabber.
TEST_F(GeminiContainerUIStateManagerTest,
       TestRespondingAfterIntervalStaysMinimizedWithGrabber) {
  [state_manager_ setupInitialUIState];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kThinking];

  task_environment_.FastForwardBy(
      base::Seconds(GetGeminiResponseReadyInterval() + 1));

  [delegate_ reset];
  [state_manager_ transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                                   kResponding];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);
}

// Tests that thinking timer resets if the next change after thinking is a mode
// change rather than responding.
TEST_F(GeminiContainerUIStateManagerTest,
       TestThinkingTimerResetsIfNextChangeIsModeChange) {
  [state_manager_ setupInitialUIState];

  [delegate_ reset];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kThinking];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_FALSE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);

  // Next change is a mode change to live mode (not responding).
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kLive];

  // Switch back to floaty.
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kFloaty];
  state_manager_.currentUIState = {
      .detent = AssistantContainerDetent::kMinimized,
      .hasGrabber = NO,
      .zeroStateVisible = NO,
  };

  // When responding status arrives, timer was reset so container stays
  // minimized.
  [delegate_ reset];
  [state_manager_ transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                                   kResponding];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
}

// Tests that thinking timer resets if the next change after thinking is a
// status change other than responding (e.g. dormant).
TEST_F(GeminiContainerUIStateManagerTest,
       TestThinkingTimerResetsIfNextChangeIsDormantStatus) {
  [state_manager_ setupInitialUIState];

  [delegate_ reset];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kThinking];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_FALSE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);

  // Next change is dormant status (not responding).
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kDormant];

  // When responding subsequently arrives within interval, timer was reset so
  // container stays minimized.
  task_environment_.FastForwardBy(
      base::Seconds(GetGeminiResponseReadyInterval() - 1));
  [delegate_ reset];
  [state_manager_ transitionToProcessingStatus:ios::provider::GeminiClientMode::
                                                   kResponding];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
}

// Tests that handleNewChat preserves the container detent and restores
// zero state.
TEST_F(GeminiContainerUIStateManagerTest, TestHandleNewChatPreservesDetent) {
  [state_manager_ setupInitialUIState];
  [state_manager_ updateDetent:AssistantContainerDetent::kMinimized];
  state_manager_.hasConversation = YES;

  [delegate_ reset];
  [state_manager_ handleNewChat];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_TRUE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_FALSE(state_manager_.hasConversation);
}

// Tests that response cancellation with stop button sets expanded response.
TEST_F(GeminiContainerUIStateManagerTest, TestCancelWithStopButton) {
  [state_manager_ setupInitialUIState];
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kThinking];
  [delegate_ reset];
  [state_manager_
      handleResponseCancellationWithReason:GeminiCancelTypeStopButtonTapped];
  EXPECT_EQ(1, delegate_.changeCount);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastUIState.detent);
  EXPECT_TRUE(delegate_.lastUIState.hasGrabber);
  EXPECT_FALSE(delegate_.lastUIState.zeroStateVisible);
  EXPECT_TRUE(state_manager_.hasConversation);
}

// Tests that response cancellation with other reasons does not return a new
// configuration.
TEST_F(GeminiContainerUIStateManagerTest, TestCancelWithOtherReason) {
  [state_manager_ setupInitialUIState];
  [delegate_ reset];
  [state_manager_ handleResponseCancellationWithReason:
                      GeminiCancelTypeCollapsedStateCloseButtonTapped];
  EXPECT_EQ(0, delegate_.changeCount);
}

// Tests that updateDetent updates the current detent.
TEST_F(GeminiContainerUIStateManagerTest, TestUpdateDetent) {
  [state_manager_ setupInitialUIState];
  [state_manager_ updateDetent:AssistantContainerDetent::kLarge];
  EXPECT_EQ(AssistantContainerDetent::kLarge,
            state_manager_.currentUIState.detent);
}

// Tests that reset restores state manager properties.
TEST_F(GeminiContainerUIStateManagerTest, TestReset) {
  [state_manager_ setupInitialUIState];
  state_manager_.hasConversation = YES;
  [state_manager_ reset];
  EXPECT_EQ(ios::provider::GeminiViewMode::kUnknown, state_manager_.viewMode);
  EXPECT_EQ(ios::provider::GeminiClientMode::kUnknown,
            state_manager_.processingStatus);
  EXPECT_FALSE(state_manager_.hasConversation);
}

// Tests that shouldBeDismissed returns YES only when Floaty is minimized in
// zero state with Chrome Next IA enabled.
TEST_F(GeminiContainerUIStateManagerTest, TestShouldBeDismissed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kChromeNextIa);

  [state_manager_ setupInitialUIState];
  // Floaty mode at medium detent should not be dismissed.
  EXPECT_FALSE([state_manager_ shouldBeDismissed]);

  // Floaty mode at minimized detent in zero state should be dismissed.
  [state_manager_ updateDetent:AssistantContainerDetent::kMinimized];
  EXPECT_TRUE([state_manager_ shouldBeDismissed]);

  // Floaty mode with active conversation at minimized detent should not be
  // dismissed.
  [state_manager_
      transitionToProcessingStatus:ios::provider::GeminiClientMode::kThinking];
  EXPECT_FALSE([state_manager_ shouldBeDismissed]);

  // Resetting conversation via new chat button at minimized detent should be
  // dismissed.
  [state_manager_ handleNewChat];
  EXPECT_TRUE([state_manager_ shouldBeDismissed]);
}

// Tests that shouldBeDismissed returns NO at minimized detent when Chrome
// Next IA is disabled even in zero state.
TEST_F(GeminiContainerUIStateManagerTest, TestShouldBeDismissedNextIaDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kChromeNextIa);

  [state_manager_ setupInitialUIState];
  [state_manager_ updateDetent:AssistantContainerDetent::kMinimized];
  EXPECT_FALSE([state_manager_ shouldBeDismissed]);
}

// Tests that shouldBeDismissed returns NO when view mode is not Floaty even at
// minimized detent in zero state.
TEST_F(GeminiContainerUIStateManagerTest, TestShouldBeDismissedNonFloatyMode) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kChromeNextIa);

  [state_manager_ setupInitialUIState];
  [state_manager_ transitionToMode:ios::provider::GeminiViewMode::kLive];
  [state_manager_ updateDetent:AssistantContainerDetent::kMinimized];
  EXPECT_FALSE([state_manager_ shouldBeDismissed]);
}
