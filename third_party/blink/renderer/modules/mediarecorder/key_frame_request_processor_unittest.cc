// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/key_frame_request_processor.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class KeyFrameRequestProcessorClockTest : public ::testing::Test {
 public:
  base::TimeTicks Now() const { return now_; }
  void Advance(base::TimeDelta duration) { now_ += duration; }
  void OnKeyFrame(KeyFrameRequestProcessor& processor) {
    processor.OnKeyFrame(Now());
  }
  bool OnFrameAndShouldRequestKeyFrame(KeyFrameRequestProcessor& processor) {
    return processor.OnFrameAndShouldRequestKeyFrame(Now());
  }

 private:
  test::TaskEnvironment task_environment_;
  base::TimeTicks now_;
};

TEST(KeyFrameRequestProcessorTest, DefaultConfigurationIsUnconfigured) {
  test::TaskEnvironment task_environment;
  KeyFrameRequestProcessor::Configuration config;
  ASSERT_TRUE(
      absl::get_if<KeyFrameRequestProcessor::NotConfiguredTag>(&config));
}

TEST_F(KeyFrameRequestProcessorClockTest,
       DefaultProcessorRequestKeyFrameEvery100Frames) {
  KeyFrameRequestProcessor processor;
  OnKeyFrame(processor);
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  for (int i = 0; i != 99; i++) {
    Advance(base::Seconds(1));
    ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  }
  // 101-th frame.
  ASSERT_TRUE(OnFrameAndShouldRequestKeyFrame(processor));
  // No keyframe during 24 hours of runtime.
  Advance(base::Hours(24));
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
}

TEST_F(KeyFrameRequestProcessorClockTest,
       CountIntervalSuggestsKeyframesPeriodically) {
  KeyFrameRequestProcessor processor(2u);
  OnKeyFrame(processor);
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  ASSERT_TRUE(OnFrameAndShouldRequestKeyFrame(processor));

  OnKeyFrame(processor);
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  ASSERT_TRUE(OnFrameAndShouldRequestKeyFrame(processor));
}

TEST_F(KeyFrameRequestProcessorClockTest,
       DurationIntervalSuggestsKeyframesPeriodically) {
  KeyFrameRequestProcessor processor(base::Seconds(1));
  OnKeyFrame(processor);
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  Advance(base::Milliseconds(500));
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  Advance(base::Milliseconds(500));
  ASSERT_TRUE(OnFrameAndShouldRequestKeyFrame(processor));

  OnKeyFrame(processor);
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  Advance(base::Milliseconds(500));
  ASSERT_FALSE(OnFrameAndShouldRequestKeyFrame(processor));
  Advance(base::Milliseconds(500));
  ASSERT_TRUE(OnFrameAndShouldRequestKeyFrame(processor));
}

}  // namespace
}  // namespace blink
