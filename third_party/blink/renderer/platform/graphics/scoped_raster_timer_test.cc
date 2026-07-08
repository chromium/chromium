// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/scoped_raster_timer.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using testing::Test;

constexpr base::TimeDelta kExpectedCPUDuration =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime;
// kExpectedGPUDuration does not need to be related to kMockElapsedTime.
// We chose kMockElapsedTime * 2 arbitrarily to ensure that CPU, GPU, and
// Total duration values all end up in different histogram buckets.
constexpr base::TimeDelta kExpectedGPUDuration =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime * 2;

// This is a fake raster interface that will always report that GPU
// commands have finished executing in kExpectedGPUDuration microseconds.
class FakeRasterCommandsCompleted : public viz::TestRasterInterface {
 public:
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override {
    if (pname == GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT) {
      // Signal that commands have completed.
      *params = 1;
    } else if (pname == GL_QUERY_RESULT_EXT) {
      *params = kExpectedGPUDuration.InMicroseconds();
    } else {
      viz::TestRasterInterface::GetQueryObjectuivEXT(id, pname, params);
    }
  }
};

class ScopedRasterTimerTest : public Test {};

TEST_F(ScopedRasterTimerTest, UnacceleratedRasterDuration) {
  base::ScopedMockElapsedTimersForTest mock_timer;
  ScopedRasterTimer::Host host;
  base::HistogramTester histograms;

  {
    ScopedRasterTimer timer(nullptr, host, /*always_measure_for_testing=*/true);
  }

  histograms.ExpectUniqueSample(
      ScopedRasterTimer::kRasterDurationUnacceleratedHistogram,
      kExpectedCPUDuration.InMicroseconds(), 1);
  histograms.ExpectTotalCount(
      ScopedRasterTimer::kRasterDurationAcceleratedCpuHistogram, 0);
  histograms.ExpectTotalCount(
      ScopedRasterTimer::kRasterDurationAcceleratedGpuHistogram, 0);
  histograms.ExpectTotalCount(
      ScopedRasterTimer::kRasterDurationAcceleratedTotalHistogram, 0);
}

TEST_F(ScopedRasterTimerTest, AcceleratedRasterDuration) {
  base::ScopedMockElapsedTimersForTest mock_timer;
  ScopedRasterTimer::Host host;
  base::HistogramTester histograms;

  FakeRasterCommandsCompleted fake_raster;

  {
    ScopedRasterTimer timer(&fake_raster, host,
                            /*always_measure_for_testing=*/true);
  }

  host.CheckGpuTimers(&fake_raster);

  histograms.ExpectTotalCount(
      ScopedRasterTimer::kRasterDurationUnacceleratedHistogram, 0);
  histograms.ExpectUniqueSample(
      ScopedRasterTimer::kRasterDurationAcceleratedCpuHistogram,
      kExpectedCPUDuration.InMicroseconds(), 1);
  histograms.ExpectUniqueSample(
      ScopedRasterTimer::kRasterDurationAcceleratedGpuHistogram,
      kExpectedGPUDuration.InMicroseconds(), 1);
  histograms.ExpectUniqueSample(
      ScopedRasterTimer::kRasterDurationAcceleratedTotalHistogram,
      (kExpectedCPUDuration + kExpectedGPUDuration).InMicroseconds(), 1);
}

}  // namespace blink
