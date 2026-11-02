// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/key_frame_request_processor.h"

#include <variant>

#include "base/functional/bind.h"
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
  ASSERT_TRUE(std::get_if<KeyFrameRequestProcessor::NotConfiguredTag>(&config));
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
  KeyFrameRequestProcessor processor(
      2u, /*cross_origin_isolated_capability=*/false);
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
  KeyFrameRequestProcessor processor(
      base::Seconds(1), /*cross_origin_isolated_capability=*/false);
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

// Requests a key frame whenever allowed, stepping 1 us over (1000, 1100] us
// with a 1 us interval. Returns the number of requests.
int CountRequestsOver100us(bool cross_origin_isolated_capability) {
  KeyFrameRequestProcessor processor(base::Microseconds(1),
                                     cross_origin_isolated_capability);
  const base::TimeTicks origin = base::TimeTicks() + base::Microseconds(1000);
  processor.OnKeyFrame(origin);
  int requests = 0;
  for (int i = 1; i <= 100; ++i) {
    const base::TimeTicks now = origin + base::Microseconds(i);
    if (processor.OnFrameAndShouldRequestKeyFrame(now)) {
      ++requests;
      processor.OnKeyFrame(now);
    }
  }
  return requests;
}

TEST(KeyFrameRequestProcessorTest, DurationIntervalIsCoarsened) {
  test::TaskEnvironment task_environment;
  // (1000, 1100] us is one 100 us bucket ending on a multiple, so exactly one
  // request fires regardless of the clamper's jitter.
  EXPECT_EQ(CountRequestsOver100us(/*cross_origin_isolated_capability=*/false),
            1);
  // 5 us resolution when cross-origin isolated.
  EXPECT_EQ(CountRequestsOver100us(/*cross_origin_isolated_capability=*/true),
            20);
}

}  // namespace
}  // namespace blink
