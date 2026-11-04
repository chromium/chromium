// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_waveform_view.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class TtcWaveformViewTest : public PlatformTest {
 protected:
  TtcWaveformViewTest() {
    view_ = [[TTCWaveformView alloc]
        initWithFrame:CGRectMake(0.0, 0.0, 63.0, 24.0)];
  }

  TTCWaveformView* view_ = nil;
};

// Tests that `TTCWaveformView` initializes in `kDots` state with zero audio
// level and its default intrinsic content size.
TEST_F(TtcWaveformViewTest, InitializesWithDotsStateAndDefaultContentSize) {
  EXPECT_EQ(view_.state, TTCWaveformState::kDots);
  EXPECT_FLOAT_EQ(view_.audioLevel, 0.0f);
  EXPECT_EQ(view_.intrinsicContentSize.width, 63.0);
  EXPECT_EQ(view_.intrinsicContentSize.height, 24.0);
  EXPECT_NSEQ(view_.accessibilityIdentifier,
              kTTCWaveformViewAccessibilityIdentifier);
  EXPECT_FALSE(view_.userInteractionEnabled);
}

// Tests that `audioLevel` clamps out-of-range inputs to `[0.0, 1.0]`.
TEST_F(TtcWaveformViewTest, SetAudioLevelClampsToNormalizedRange) {
  struct AudioLevelTestCase {
    float input;
    float expected;
  };
  constexpr AudioLevelTestCase kTestCases[] = {
      {0.65f, 0.65f},
      {-0.5f, 0.0f},
      {1.5f, 1.0f},
  };

  for (const AudioLevelTestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input);
    view_.audioLevel = test_case.input;

    EXPECT_FLOAT_EQ(view_.audioLevel, test_case.expected);
  }
}

}  // namespace
