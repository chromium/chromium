// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/audio_power_monitor.h"

#include <limits>
#include <memory>

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kSampleRate = 48000;
static const int kFramesPerBuffer = 128;

static const int kTimeConstantMillis = 5;

namespace {

// Container for each parameterized test's data (input and expected results).
class TestScenario {
 public:
  TestScenario(const float* data,
               int num_channels,
               int num_frames,
               float expected_power,
               bool expected_clipped)
      : expected_power_(expected_power), expected_clipped_(expected_clipped) {
    CreatePopulatedBuffer(data, num_channels, num_frames);
  }

  // Copy constructor and assignment operator for ::testing::Values(...).
  TestScenario(const TestScenario& other) { *this = other; }
  TestScenario& operator=(const TestScenario& other) {
    this->expected_power_ = other.expected_power_;
    this->expected_clipped_ = other.expected_clipped_;
    this->bus_ = AudioBus::Create(other.bus_->channels(), other.bus_->frames());
    other.bus_->CopyTo(this->bus_.get());
    return *this;
  }

  // Returns this TestScenario, but with a bad sample value placed in the middle
  // of channel 0.
  TestScenario WithABadSample(float bad_value) const {
    TestScenario result(*this);
    result.bus_->channel(0)[result.bus_->frames() / 2] = bad_value;
    return result;
  }

  const AudioBus& data() const { return *bus_; }

  float expected_power() const { return expected_power_; }

  bool expected_clipped() const { return expected_clipped_; }

 private:
  // Creates an AudioBus, sized and populated with kFramesPerBuffer frames of
  // data.  The given test |data| is repeated to fill the buffer.
  void CreatePopulatedBuffer(const float* data,
                             int num_channels,
                             int num_frames) {
    bus_ = AudioBus::Create(num_channels, kFramesPerBuffer);
    for (int ch = 0; ch < num_channels; ++ch) {
      for (int frames = 0; frames < kFramesPerBuffer; frames += num_frames) {
        const int num_to_copy = std::min(num_frames, kFramesPerBuffer - frames);
        memcpy(bus_->channel(ch) + frames, data + num_frames * ch,
               sizeof(float) * num_to_copy);
      }
    }
  }

  float expected_power_;
  bool expected_clipped_;
  std::unique_ptr<AudioBus> bus_;
};

// Value printer for TestScenario.  Required to prevent Valgrind "access to
// uninitialized memory" errors (http://crbug.com/263315).
::std::ostream& operator<<(::std::ostream& os, const TestScenario& ts) {
  return os << "{" << ts.data().channels() << "-channel signal} --> {"
            << ts.expected_power() << " dBFS, "
            << (ts.expected_clipped() ? "clipped" : "not clipped") << "}";
}

// An observer that receives power measurements.  Each power measurement should
// should make progress towards the goal value.
class MeasurementObserver {
 public:
  explicit MeasurementObserver(float goal_power_measurement)
      : goal_power_measurement_(goal_power_measurement),
        measurement_count_(0),
        last_power_measurement_(AudioPowerMonitor::zero_power()),
        last_clipped_(false) {}

  MeasurementObserver(const MeasurementObserver&) = delete;
  MeasurementObserver& operator=(const MeasurementObserver&) = delete;

  int measurement_count() const { return measurement_count_; }

  float last_power_measurement() const { return last_power_measurement_; }

  bool last_clipped() const { return last_clipped_; }

  void OnPowerMeasured(float cur_power_measurement, bool clipped) {
    if (measurement_count_ == 0) {
      measurements_should_increase_ =
          (cur_power_measurement < goal_power_measurement_);
    } else {
      SCOPED_TRACE(::testing::Message()
                   << "Power: goal=" << goal_power_measurement_
                   << "; last=" << last_power_measurement_
                   << "; cur=" << cur_power_measurement);

      if (last_power_measurement_ != goal_power_measurement_) {
        if (measurements_should_increase_) {
          EXPECT_LE(last_power_measurement_, cur_power_measurement)
              << "Measurements should be monotonically increasing.";
        } else {
          EXPECT_GE(last_power_measurement_, cur_power_measurement)
              << "Measurements should be monotonically decreasing.";
        }
      } else {
        EXPECT_EQ(last_power_measurement_, cur_power_measurement)
            << "Measurements are numerically unstable at goal value.";
      }
    }

    last_power_measurement_ = cur_power_measurement;
    last_clipped_ = clipped;
    ++measurement_count_;
  }

 private:
  const float goal_power_measurement_;
  int measurement_count_;
  bool measurements_should_increase_;
  float last_power_measurement_;
  bool last_clipped_;
};

}  // namespace

class AudioPowerMonitorTest : public ::testing::TestWithParam<TestScenario> {
 public:
  AudioPowerMonitorTest()
      : power_monitor_(kSampleRate, base::Milliseconds(kTimeConstantMillis)) {}

  AudioPowerMonitorTest(const AudioPowerMonitorTest&) = delete;
  AudioPowerMonitorTest& operator=(const AudioPowerMonitorTest&) = delete;

  void FeedAndCheckExpectedPowerIsMeasured(const AudioBus& bus,
                                           float power,
                                           bool clipped) {
    // Feed the AudioPowerMonitor, read measurements from it, and record them in
    // MeasurementObserver.
    static const int kNumFeedIters = 100;
    MeasurementObserver observer(power);
    for (int i = 0; i < kNumFeedIters; ++i) {
      power_monitor_.Scan(bus, bus.frames());
      const std::pair<float, bool>& reading =
          power_monitor_.ReadCurrentPowerAndClip();
      observer.OnPowerMeasured(reading.first, reading.second);
    }

    // Check that the results recorded by the observer are the same whole-number
    // dBFS.
    EXPECT_EQ(static_cast<int>(power),
              static_cast<int>(observer.last_power_measurement()));
    EXPECT_EQ(clipped, observer.last_clipped());
  }

 private:
  AudioPowerMonitor power_monitor_;
};

TEST_P(AudioPowerMonitorTest, MeasuresPowerOfSignal) {
  const TestScenario& scenario = GetParam();

  std::unique_ptr<AudioBus> zeroed_bus =
      AudioBus::Create(scenario.data().channels(), scenario.data().frames());
  zeroed_bus->Zero();

  // Send a "zero power" audio signal, then this scenario's audio signal, then
  // the "zero power" audio signal again; testing that the power monitor
  // measurements match expected values.
  FeedAndCheckExpectedPowerIsMeasured(*zeroed_bus,
                                      AudioPowerMonitor::zero_power(), false);
  FeedAndCheckExpectedPowerIsMeasured(
      scenario.data(), scenario.expected_power(), scenario.expected_clipped());
  FeedAndCheckExpectedPowerIsMeasured(*zeroed_bus,
                                      AudioPowerMonitor::zero_power(), false);
}

static const float kMonoSilentNoise[] = {0.01f, -0.01f};

static const float kMonoMaxAmplitude[] = {1.0f};

static const float kMonoMaxAmplitude2[] = {-1.0f, 1.0f};

static const float kMonoHalfMaxAmplitude[] = {0.5f, -0.5f, 0.5f, -0.5f};

static const float kMonoAmplitudeClipped[] = {2.0f, -2.0f};

static const float kMonoMaxAmplitudeWithClip[] = {2.0f, 0.0, 0.0f, 0.0f};

static const float kMonoMaxAmplitudeWithClip2[] = {4.0f, 0.0, 0.0f, 0.0f};

static const float kStereoSilentNoise[] = {
    // left channel
    0.005f, -0.005f,
    // right channel
    0.005f, -0.005f};

static const float kStereoMaxAmplitude[] = {
    // left channel
    1.0f, -1.0f,
    // right channel
    -1.0f, 1.0f};

static const float kRightChannelMaxAmplitude[] = {
    // left channel
    0.0f, 0.0f, 0.0f, 0.0f,
    // right channel
    -1.0f, 1.0f, -1.0f, 1.0f};

static const float kLeftChannelHalfMaxAmplitude[] = {
    // left channel
    0.5f,
    -0.5f,
    0.5f,
    -0.5f,
    // right channel
    0.0f,
    0.0f,
    0.0f,
    0.0f,
};

static const float kStereoMixed[] = {
    // left channel
    0.5f, -0.5f, 0.5f, -0.5f,
    // right channel
    -1.0f, 1.0f, -1.0f, 1.0f};

static const float kStereoMixed2[] = {
    // left channel
    1.0f, -1.0f, 0.75f, -0.75f, 0.5f, -0.5f, 0.25f, -0.25f,
    // right channel
    0.25f, -0.25f, 0.5f, -0.5f, 0.75f, -0.75f, 1.0f, -1.0f};

INSTANTIATE_TEST_SUITE_P(
    Scenarios,
    AudioPowerMonitorTest,
    ::testing::Values(
        TestScenario(kMonoSilentNoise, 1, 2, -40, false),
        TestScenario(kMonoMaxAmplitude,
                     1,
                     1,
                     AudioPowerMonitor::max_power(),
                     false),
        TestScenario(kMonoMaxAmplitude2,
                     1,
                     2,
                     AudioPowerMonitor::max_power(),
                     false),
        TestScenario(kMonoHalfMaxAmplitude, 1, 4, -6, false),
        TestScenario(kMonoAmplitudeClipped,
                     1,
                     2,
                     AudioPowerMonitor::max_power(),
                     true),
        TestScenario(kMonoMaxAmplitudeWithClip,
                     1,
                     4,
                     AudioPowerMonitor::max_power(),
                     true),
        TestScenario(kMonoMaxAmplitudeWithClip2,
                     1,
                     4,
                     AudioPowerMonitor::max_power(),
                     true),
        TestScenario(kMonoSilentNoise,
                     1,
                     2,
                     AudioPowerMonitor::zero_power(),
                     false)
            .WithABadSample(std::numeric_limits<float>::infinity()),
        TestScenario(kMonoHalfMaxAmplitude,
                     1,
                     4,
                     AudioPowerMonitor::zero_power(),
                     false)
            .WithABadSample(std::numeric_limits<float>::quiet_NaN()),
        TestScenario(kStereoSilentNoise, 2, 2, -46, false),
        TestScenario(kStereoMaxAmplitude,
                     2,
                     2,
                     AudioPowerMonitor::max_power(),
                     false),
        TestScenario(kRightChannelMaxAmplitude, 2, 4, -3, false),
        TestScenario(kLeftChannelHalfMaxAmplitude, 2, 4, -9, false),
        TestScenario(kStereoMixed, 2, 4, -2, false),
        TestScenario(kStereoMixed2, 2, 8, -3, false)));

}  // namespace media
