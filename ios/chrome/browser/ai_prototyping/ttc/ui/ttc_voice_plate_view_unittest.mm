// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_view.h"

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_mutator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_waveform_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class TtcVoicePlateViewTest : public PlatformTest {
 protected:
  TtcVoicePlateViewTest() {
    mock_mutator_ = OCMProtocolMock(@protocol(TTCVoicePlateMutator));
    view_ = [[TTCVoicePlateView alloc] initWithFrame:CGRectZero];
    view_.mutator = mock_mutator_;
  }

  // Traverses subviews to find the view matching `identifier`.
  UIView* FindSubviewWithAccessibilityIdentifier(UIView* root,
                                                 NSString* identifier) {
    if ([root.accessibilityIdentifier isEqualToString:identifier]) {
      return root;
    }
    for (UIView* subview in root.subviews) {
      UIView* found =
          FindSubviewWithAccessibilityIdentifier(subview, identifier);
      if (found) {
        return found;
      }
    }
    return nil;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  id<TTCVoicePlateMutator> mock_mutator_ = nil;
  TTCVoicePlateView* view_ = nil;
};

// Tests that `TTCVoicePlateView` initializes in idle state with the pause
// button disabled and the start session button enabled.
TEST_F(TtcVoicePlateViewTest, InitializesInIdleStateWithStartButtonEnabled) {
  UIButton* play_pause_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier));
  ASSERT_NE(play_pause_button, nil);
  EXPECT_FALSE(play_pause_button.enabled);
  EXPECT_NSEQ(play_pause_button.accessibilityLabel, @"Pause session");
  EXPECT_NSEQ(play_pause_button.backgroundColor,
              [UIColor colorNamed:kTertiaryBackgroundColor]);

  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  ASSERT_NE(close_button, nil);
  EXPECT_TRUE(close_button.enabled);
  EXPECT_NSEQ(close_button.accessibilityLabel, @"Start session");
  EXPECT_NSEQ(close_button.backgroundColor, [UIColor colorNamed:kBlueColor]);

  UIView* error_label = FindSubviewWithAccessibilityIdentifier(
      view_, kTTCVoicePlateErrorBannerAccessibilityIdentifier);
  ASSERT_NE(error_label, nil);
  EXPECT_TRUE(error_label.superview.hidden);
}

// Tests that tapping the right session button in idle state dispatches
// `startSession` to start the session.
TEST_F(TtcVoicePlateViewTest, SessionButtonWhenIdleDispatchesStartSession) {
  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  ASSERT_NE(close_button, nil);
  OCMExpect([mock_mutator_ startSession]);

  [close_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that tapping the right session button in error state dispatches
// `startSession` to retry starting the session.
TEST_F(TtcVoicePlateViewTest, SessionButtonWhenErrorDispatchesStartSession) {
  [view_ setSessionState:TTCSessionUIState::kError];
  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  ASSERT_NE(close_button, nil);
  OCMExpect([mock_mutator_ startSession]);

  [close_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that tapping the left microphone button when listening dispatches
// `toggleMicMuted` to the mutator.
TEST_F(TtcVoicePlateViewTest, PlayPauseButtonDispatchesToggleMicMuted) {
  [view_ setSessionState:TTCSessionUIState::kListening];
  UIButton* play_pause_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier));
  ASSERT_NE(play_pause_button, nil);
  OCMExpect([mock_mutator_ toggleMicMuted]);

  [play_pause_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that tapping the right session button when active dispatches
// `stopSession` to the mutator.
TEST_F(TtcVoicePlateViewTest, CloseButtonDispatchesStopSession) {
  [view_ setSessionState:TTCSessionUIState::kListening];
  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  ASSERT_NE(close_button, nil);
  OCMExpect([mock_mutator_ stopSession]);

  [close_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that setting the session state to connecting transitions the waveform
// to processing, keeps the microphone button disabled, and switches the session
// button to close.
TEST_F(TtcVoicePlateViewTest,
       SetSessionStateConnectingShowsProcessingWaveformAndCloseButton) {
  UIButton* play_pause_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier));
  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(play_pause_button, nil);
  ASSERT_NE(close_button, nil);
  ASSERT_NE(waveform_view, nil);

  [view_ setSessionState:TTCSessionUIState::kConnecting];

  EXPECT_EQ(waveform_view.state, TTCWaveformState::kProcessing);
  EXPECT_FALSE(play_pause_button.enabled);
  EXPECT_NSEQ(play_pause_button.backgroundColor,
              [UIColor colorNamed:kTertiaryBackgroundColor]);
  EXPECT_NSEQ(close_button.accessibilityLabel, @"Close session");
}

// Tests that setting the session state to listening enables the pause button
// and transitions the waveform to listening state.
TEST_F(TtcVoicePlateViewTest,
       SetSessionStateListeningEnablesPauseButtonAndListeningWaveform) {
  UIButton* play_pause_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier));
  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(play_pause_button, nil);
  ASSERT_NE(close_button, nil);
  ASSERT_NE(waveform_view, nil);

  [view_ setSessionState:TTCSessionUIState::kListening];

  EXPECT_EQ(waveform_view.state, TTCWaveformState::kListening);
  EXPECT_TRUE(play_pause_button.enabled);
  EXPECT_NSEQ(play_pause_button.accessibilityLabel, @"Pause session");
  EXPECT_NSEQ(play_pause_button.backgroundColor,
              [UIColor colorNamed:kTertiaryBackgroundColor]);
  EXPECT_NSEQ(close_button.accessibilityLabel, @"Close session");
}

// Tests that muting while listening switches the microphone button to a blue
// resume button and returns the waveform to dots state.
TEST_F(TtcVoicePlateViewTest,
       SetMicMutedWhileListeningShowsResumeButtonAndDotsWaveform) {
  UIButton* play_pause_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier));
  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(play_pause_button, nil);
  ASSERT_NE(close_button, nil);
  ASSERT_NE(waveform_view, nil);
  [view_ setSessionState:TTCSessionUIState::kListening];

  [view_ setMicMuted:YES];

  EXPECT_EQ(waveform_view.state, TTCWaveformState::kDots);
  EXPECT_TRUE(play_pause_button.enabled);
  EXPECT_NSEQ(play_pause_button.accessibilityLabel, @"Resume session");
  EXPECT_NSEQ(play_pause_button.backgroundColor,
              [UIColor colorNamed:kBlueColor]);
  EXPECT_NSEQ(close_button.accessibilityLabel, @"Close session");
}

// Tests that transitioning to error state resets the controls to start session
// appearance with the microphone button disabled.
TEST_F(TtcVoicePlateViewTest, SetSessionStateErrorResetsControlsToStartState) {
  UIButton* play_pause_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier));
  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(play_pause_button, nil);
  ASSERT_NE(close_button, nil);
  ASSERT_NE(waveform_view, nil);
  [view_ setSessionState:TTCSessionUIState::kListening];

  [view_ setSessionState:TTCSessionUIState::kError];

  EXPECT_EQ(waveform_view.state, TTCWaveformState::kDots);
  EXPECT_FALSE(play_pause_button.enabled);
  EXPECT_NSEQ(play_pause_button.accessibilityLabel, @"Pause session");
  EXPECT_NSEQ(play_pause_button.backgroundColor,
              [UIColor colorNamed:kTertiaryBackgroundColor]);
  EXPECT_NSEQ(close_button.accessibilityLabel, @"Start session");
  EXPECT_NSEQ(close_button.backgroundColor, [UIColor colorNamed:kBlueColor]);
}

// Tests that setting the session state back to idle resets the controls to
// start session appearance.
TEST_F(TtcVoicePlateViewTest, SetSessionStateIdleResetsControlsToStartState) {
  UIButton* play_pause_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier));
  UIButton* close_button =
      base::apple::ObjCCast<UIButton>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateCloseButtonAccessibilityIdentifier));
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(play_pause_button, nil);
  ASSERT_NE(close_button, nil);
  ASSERT_NE(waveform_view, nil);
  [view_ setSessionState:TTCSessionUIState::kListening];

  [view_ setSessionState:TTCSessionUIState::kIdle];

  EXPECT_EQ(waveform_view.state, TTCWaveformState::kDots);
  EXPECT_FALSE(play_pause_button.enabled);
  EXPECT_NSEQ(play_pause_button.accessibilityLabel, @"Pause session");
  EXPECT_NSEQ(play_pause_button.backgroundColor,
              [UIColor colorNamed:kTertiaryBackgroundColor]);
  EXPECT_NSEQ(close_button.accessibilityLabel, @"Start session");
  EXPECT_NSEQ(close_button.backgroundColor, [UIColor colorNamed:kBlueColor]);
}

// Tests that mic energy level updates the waveform.
TEST_F(TtcVoicePlateViewTest, SetMicEnergyLevelUpdatesWaveform) {
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(waveform_view, nil);

  [view_ setMicEnergyLevel:0.75f];

  EXPECT_FLOAT_EQ(waveform_view.audioLevel, 0.75f);
}

// Tests that showing an error message displays the transient chip and
// auto-dismisses after 4 seconds.
TEST_F(TtcVoicePlateViewTest,
       ShowErrorMessageDisplaysChipAndAutoDismissesAfterTimeout) {
  UILabel* error_label =
      base::apple::ObjCCast<UILabel>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateErrorBannerAccessibilityIdentifier));
  ASSERT_NE(error_label, nil);
  ASSERT_TRUE(error_label.superview.hidden);

  [view_ showErrorMessage:@"Network timeout"];
  EXPECT_FALSE(error_label.superview.hidden);
  EXPECT_NSEQ(error_label.text, @"Network timeout");

  task_environment_.FastForwardBy(base::Seconds(4));

  EXPECT_TRUE(error_label.superview.hidden);
  EXPECT_EQ(error_label.text, nil);
}

// Tests that transitioning to a non-error state immediately dismisses an active
// error chip before the 4-second timer elapses.
TEST_F(TtcVoicePlateViewTest, NonErrorStateDismissesActiveErrorChip) {
  UILabel* error_label =
      base::apple::ObjCCast<UILabel>(FindSubviewWithAccessibilityIdentifier(
          view_, kTTCVoicePlateErrorBannerAccessibilityIdentifier));
  ASSERT_NE(error_label, nil);
  [view_ setSessionState:TTCSessionUIState::kError];
  [view_ showErrorMessage:@"Session error occurred"];
  ASSERT_FALSE(error_label.superview.hidden);

  [view_ setSessionState:TTCSessionUIState::kConnecting];

  EXPECT_TRUE(error_label.superview.hidden);
  EXPECT_EQ(error_label.text, nil);
}

}  // namespace
