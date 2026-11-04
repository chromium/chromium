// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_waveform_view.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class TtcVoicePlateViewTest : public PlatformTest {
 protected:
  TtcVoicePlateViewTest() {
    view_ = [[TTCVoicePlateView alloc] initWithFrame:CGRectZero];
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

  TTCVoicePlateView* view_ = nil;
};

// Tests that setting the session state to connecting transitions the waveform
// to processing state.
TEST_F(TtcVoicePlateViewTest,
       SetSessionStateConnectingShowsProcessingWaveform) {
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(waveform_view, nil);
  ASSERT_EQ(waveform_view.state, TTCWaveformState::kDots);

  [view_ setSessionState:TTCSessionUIState::kConnecting];

  EXPECT_EQ(waveform_view.state, TTCWaveformState::kProcessing);
}

// Tests that setting the session state to listening transitions the waveform
// to listening state.
TEST_F(TtcVoicePlateViewTest, SetSessionStateListeningShowsListeningWaveform) {
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(waveform_view, nil);

  [view_ setSessionState:TTCSessionUIState::kListening];

  EXPECT_EQ(waveform_view.state, TTCWaveformState::kListening);
}

// Tests that muting and unmuting while listening toggles the waveform between
// dots and listening states.
TEST_F(TtcVoicePlateViewTest,
       SetMicMutedWhileListeningTogglesDotsAndListeningWaveform) {
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(waveform_view, nil);
  [view_ setSessionState:TTCSessionUIState::kListening];

  [view_ setMicMuted:YES];
  EXPECT_EQ(waveform_view.state, TTCWaveformState::kDots);

  [view_ setMicMuted:NO];
  EXPECT_EQ(waveform_view.state, TTCWaveformState::kListening);
}

// Tests that setting the session state back to idle resets the waveform to
// dots state.
TEST_F(TtcVoicePlateViewTest, SetSessionStateIdleResetsWaveformToDots) {
  TTCWaveformView* waveform_view = base::apple::ObjCCast<TTCWaveformView>(
      FindSubviewWithAccessibilityIdentifier(
          view_, kTTCWaveformViewAccessibilityIdentifier));
  ASSERT_NE(waveform_view, nil);
  [view_ setSessionState:TTCSessionUIState::kListening];

  [view_ setSessionState:TTCSessionUIState::kIdle];

  EXPECT_EQ(waveform_view.state, TTCWaveformState::kDots);
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

}  // namespace
