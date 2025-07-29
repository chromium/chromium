// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/content/smooth_event_sampler.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

bool AddEventAndConsiderSampling(SmoothEventSampler* sampler,
                                 base::TimeTicks event_time) {
  sampler->ConsiderPresentationEvent(event_time);
  return sampler->ShouldSample();
}

void SteadyStateSampleAndAdvance(base::TimeDelta vsync,
                                 SmoothEventSampler* sampler,
                                 base::TimeTicks* t) {
  ASSERT_TRUE(AddEventAndConsiderSampling(sampler, *t));
  ASSERT_TRUE(sampler->HasUnrecordedEvent());
  sampler->RecordSample();
  ASSERT_FALSE(sampler->HasUnrecordedEvent());
  *t += vsync;
}

void SteadyStateNoSampleAndAdvance(base::TimeDelta vsync,
                                   SmoothEventSampler* sampler,
                                   base::TimeTicks* t) {
  ASSERT_FALSE(AddEventAndConsiderSampling(sampler, *t));
  ASSERT_TRUE(sampler->HasUnrecordedEvent());
  *t += vsync;
}

base::TimeTicks InitialTestTimeTicks() {
  return base::TimeTicks() + base::Seconds(1);
}


}  // namespace

// 60Hz sampled at 30Hz should produce 30Hz.  In addition, this test contains
// much more comprehensive before/after/edge-case scenarios than the others.
TEST(SmoothEventSamplerTest, Sample60HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::Seconds(1) / 30;
  const base::TimeDelta vsync = base::Seconds(1) / 60;

  SmoothEventSampler sampler(capture_period);
  base::TimeTicks t = InitialTestTimeTicks();

  // Steady state, we should capture every other vsync, indefinitely.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 20; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    ASSERT_TRUE(sampler.HasUnrecordedEvent());
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state,
  // but at a different phase.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 50Hz sampled at 30Hz should produce a sequence where some frames are skipped.
TEST(SmoothEventSamplerTest, Sample50HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::Seconds(1) / 30;
  const base::TimeDelta vsync = base::Seconds(1) / 50;

  SmoothEventSampler sampler(capture_period);
  base::TimeTicks t = InitialTestTimeTicks();

  // Steady state, we should capture 1st, 2nd and 4th frames out of every five
  // frames, indefinitely.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 20; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state
  // again.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 75Hz sampled at 30Hz should produce a sequence where some frames are skipped.
TEST(SmoothEventSamplerTest, Sample75HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::Seconds(1) / 30;
  const base::TimeDelta vsync = base::Seconds(1) / 75;

  SmoothEventSampler sampler(capture_period);
  base::TimeTicks t = InitialTestTimeTicks();

  // Steady state, we should capture 1st and 3rd frames out of every five
  // frames, indefinitely.
  SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 20; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    t += vsync;
  }

  // Now suppose we can sample again. We capture the next frame, and not the one
  // after that, and then we're back in the steady state again.
  SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 30Hz sampled at 30Hz should produce 30Hz.
TEST(SmoothEventSamplerTest, Sample30HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::Seconds(1) / 30;
  const base::TimeDelta vsync = base::Seconds(1) / 30;

  SmoothEventSampler sampler(capture_period);
  base::TimeTicks t = InitialTestTimeTicks();

  // Steady state, we should capture every vsync, indefinitely.
  for (int i = 0; i < 200; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 10; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 24Hz sampled at 30Hz should produce 24Hz.
TEST(SmoothEventSamplerTest, Sample24HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::Seconds(1) / 30;
  const base::TimeDelta vsync = base::Seconds(1) / 24;

  SmoothEventSampler sampler(capture_period);
  base::TimeTicks t = InitialTestTimeTicks();

  // Steady state, we should capture every vsync, indefinitely.
  for (int i = 0; i < 200; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 10; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }
}

// Tests that changing the minimum capture period during usage results in the
// desired behavior.
TEST(SmoothEventSamplerTest, Sample60HertzWithVariedCapturePeriods) {
  const base::TimeDelta vsync = base::Seconds(1) / 60;
  const base::TimeDelta one_to_one_period = vsync;
  const base::TimeDelta two_to_one_period = vsync * 2;
  const base::TimeDelta two_and_three_to_one_period = base::Seconds(1) / 24;

  SmoothEventSampler sampler(one_to_one_period);
  base::TimeTicks t = InitialTestTimeTicks();

  // With the capture rate at 60 Hz, we should capture every vsync.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now change to the capture rate to 30 Hz, and we should capture every other
  // vsync.
  sampler.SetMinCapturePeriod(two_to_one_period);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now change the capture rate back to 60 Hz, and we should capture every
  // vsync again.
  sampler.SetMinCapturePeriod(one_to_one_period);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now change the capture rate to 24 Hz, and we should capture with a 2-3-2-3
  // cadence.
  sampler.SetMinCapturePeriod(two_and_three_to_one_period);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }
}

namespace {

struct DataPoint {
  bool should_capture;
  double increment_ms;
};

void ReplayCheckingSamplerDecisions(const DataPoint* data_points,
                                    size_t num_data_points,
                                    SmoothEventSampler* sampler) {
  base::TimeTicks t = InitialTestTimeTicks();
  for (size_t i = 0; i < num_data_points; ++i) {
    t += base::Microseconds(
        static_cast<int64_t>(data_points[i].increment_ms * 1000));
    ASSERT_EQ(data_points[i].should_capture,
              AddEventAndConsiderSampling(sampler, t))
        << "at data_points[" << i << ']';
    if (data_points[i].should_capture)
      sampler->RecordSample();
  }
}

}  // namespace

TEST(SmoothEventSamplerTest, DrawingAt24FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: Initial instability as a 24 FPS video was
  // started from a still screen, then clearly followed by steady-state.
  static const DataPoint data_points[] = {{true, 1437.93},
                                          {true, 150.484},
                                          {true, 217.362},
                                          {true, 50.161},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 16.721},
                                          {true, 66.88},
                                          {true, 50.161},
                                          {false, 0},
                                          {false, 0},
                                          {true, 50.16},
                                          {true, 33.441},
                                          {true, 16.72},
                                          {false, 16.72},
                                          {true, 117.041},
                                          {true, 16.72},
                                          {false, 16.72},
                                          {true, 50.161},
                                          {true, 50.16},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {true, 16.72},
                                          {false, 0},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 33.44},
                                          {true, 16.72},
                                          {false, 16.721},
                                          {true, 66.881},
                                          {false, 0},
                                          {true, 33.441},
                                          {true, 16.72},
                                          {true, 50.16},
                                          {true, 16.72},
                                          {false, 16.721},
                                          {true, 50.161},
                                          {true, 50.16},
                                          {false, 0},
                                          {true, 33.441},
                                          {true, 50.337},
                                          {true, 50.183},
                                          {true, 16.722},
                                          {true, 50.161},
                                          {true, 33.441},
                                          {true, 50.16},
                                          {true, 33.441},
                                          {true, 50.16},
                                          {true, 33.441},
                                          {true, 50.16},
                                          {true, 33.44},
                                          {true, 50.161},
                                          {true, 50.16},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {true, 50.16},
                                          {true, 50.161},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {true, 50.16},
                                          {true, 33.44},
                                          {true, 50.161},
                                          {true, 33.44},
                                          {true, 50.161},
                                          {true, 33.44},
                                          {true, 50.161},
                                          {true, 33.44},
                                          {true, 83.601},
                                          {true, 16.72},
                                          {true, 33.44},
                                          {false, 0}};

  SmoothEventSampler sampler(base::Seconds(1) / 30);
  ReplayCheckingSamplerDecisions(data_points, std::size(data_points), &sampler);
}

TEST(SmoothEventSamplerTest, DrawingAt30FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: Initial instability as a 30 FPS video was
  // started from a still screen, then followed by steady-state.  Drawing
  // framerate from the video rendering was a bit volatile, but averaged 30 FPS.
  static const DataPoint data_points[] = {{true, 2407.69},
                                          {true, 16.733},
                                          {true, 217.362},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {true, 16.721},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 50.161},
                                          {true, 50.16},
                                          {false, 0},
                                          {true, 50.161},
                                          {true, 33.44},
                                          {true, 16.72},
                                          {false, 0},
                                          {false, 16.72},
                                          {true, 66.881},
                                          {false, 0},
                                          {true, 33.44},
                                          {true, 16.72},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 33.538},
                                          {true, 33.526},
                                          {true, 33.447},
                                          {true, 33.445},
                                          {true, 33.441},
                                          {true, 16.721},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {true, 50.161},
                                          {true, 16.72},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {false, 0},
                                          {false, 16.72},
                                          {true, 66.881},
                                          {true, 16.72},
                                          {false, 16.72},
                                          {true, 50.16},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {true, 50.161},
                                          {true, 16.72},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {false, 0},
                                          {true, 66.88},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 50.16},
                                          {false, 0.001},
                                          {true, 16.721},
                                          {true, 66.88},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 50.161},
                                          {true, 16.72},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 16.72},
                                          {true, 66.881},
                                          {true, 33.44},
                                          {true, 16.72},
                                          {true, 33.441},
                                          {false, 16.72},
                                          {true, 66.88},
                                          {true, 16.721},
                                          {true, 50.16},
                                          {true, 33.44},
                                          {true, 16.72},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {true, 33.44}};

  SmoothEventSampler sampler(base::Seconds(1) / 30);
  ReplayCheckingSamplerDecisions(data_points, std::size(data_points), &sampler);
}

TEST(SmoothEventSamplerTest, DrawingAt60FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: WebGL Acquarium demo
  // (http://webglsamples.googlecode.com/hg/aquarium/aquarium.html) which ran
  // between 55-60 FPS in the steady-state.
  static const DataPoint data_points[] = {{true, 16.72},
                                          {true, 16.72},
                                          {true, 4163.29},
                                          {true, 50.193},
                                          {true, 117.041},
                                          {true, 50.161},
                                          {true, 50.16},
                                          {true, 33.441},
                                          {true, 50.16},
                                          {true, 33.44},
                                          {false, 0},
                                          {false, 0},
                                          {true, 50.161},
                                          {true, 83.601},
                                          {true, 50.16},
                                          {true, 16.72},
                                          {true, 33.441},
                                          {false, 16.72},
                                          {true, 50.16},
                                          {true, 16.72},
                                          {false, 0.001},
                                          {true, 33.441},
                                          {false, 16.72},
                                          {true, 16.72},
                                          {true, 50.16},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.441},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 16.72},
                                          {true, 16.72},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 16.721},
                                          {true, 16.721},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.441},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 16.72},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 16.721},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.441},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 16.72},
                                          {true, 16.72},
                                          {true, 50.16},
                                          {false, 0},
                                          {true, 16.721},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 16.721},
                                          {true, 16.721},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 33.441},
                                          {false, 16.72},
                                          {true, 16.72},
                                          {true, 50.16},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.441},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {false, 0},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 16.72},
                                          {true, 16.721},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.44},
                                          {true, 33.441},
                                          {false, 0},
                                          {true, 33.44},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 33.441},
                                          {false, 16.72},
                                          {true, 16.72},
                                          {true, 50.16},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.441},
                                          {false, 0},
                                          {true, 33.44},
                                          {false, 16.72},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 16.721},
                                          {true, 50.161},
                                          {false, 0},
                                          {true, 16.72},
                                          {true, 33.44},
                                          {false, 0},
                                          {true, 33.441},
                                          {false, 16.72},
                                          {true, 16.72},
                                          {true, 50.16}};

  SmoothEventSampler sampler(base::Seconds(1) / 30);
  ReplayCheckingSamplerDecisions(data_points, std::size(data_points), &sampler);
}

}  // namespace media
