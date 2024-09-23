// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/audio/snooper_node.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "services/audio/test/fake_consumer.h"
#include "services/audio/test/fake_loopback_group_member.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
namespace {

// Used to test whether the output AudioBuses have had all their values set to
// something finite.
constexpr float kInvalidAudioSample = std::numeric_limits<float>::infinity();

// The tones the source should generate into the left and right channels.
constexpr double kLeftChannelFrequency = 500.0;
constexpr double kRightChannelFrequency = 1200.0;
constexpr double kSourceVolume = 0.5;

// The duration of the audio that flows through the SnooperNode for each test.
constexpr base::TimeDelta kTestDuration = base::Seconds(10);

// The amount of time in the future where the inbound audio is being recorded.
// This simulates an audio output stream that has rendered audio that is
// scheduled to be played out in the near future.
constexpr base::TimeDelta kInputAdvanceTime = base::Milliseconds(2);

// Command-line switch to request dumping the recorded output to a WAV file for
// analyzing the recorded output from one of the tests.
constexpr std::string_view kDumpAsWavSwitch = "dump-as-wav";

// Test parameters.
struct InputAndOutputParams {
  media::AudioParameters input;
  media::AudioParameters output;
};

// Helper so that gtest can produce useful logging of the test parameters.
std::ostream& operator<<(std::ostream& out,
                         const InputAndOutputParams& test_params) {
  return out << "{input=" << test_params.input.AsHumanReadableString()
             << ", output=" << test_params.output.AsHumanReadableString()
             << "}";
}

class SnooperNodeTest : public testing::TestWithParam<InputAndOutputParams> {
 public:
  SnooperNodeTest() = default;
  ~SnooperNodeTest() override = default;

  const media::AudioParameters& input_params() const {
    return GetParam().input;
  }
  const media::AudioParameters& output_params() const {
    return GetParam().output;
  }
  base::TimeDelta output_delay() const { return output_delay_; }
  double max_relative_error() const { return max_relative_error_; }

  base::TestMockTimeTaskRunner* task_runner() const {
    return task_runner_.get();
  }
  FakeLoopbackGroupMember* group_member() { return &*group_member_; }
  SnooperNode* node() { return &*node_; }
  FakeConsumer* consumer() { return &*consumer_; }

  void SetUp() override {
    // Determine the amount of time in the past from which outbound audio should
    // be rendered. Use 20 ms as a reasonable baseline--the same as the initial
    // setting in audio::LoopbackStream--which will work for almost all normal
    // use cases.
    constexpr base::TimeDelta kBaselineOutputDelay = base::Milliseconds(20);
    output_delay_ = kBaselineOutputDelay;

    // Increase the output delay in special cases...
    if (input_params().sample_rate() < 32000) {
      // At the lower input sample rates (e.g., 8 kHz), prebufferring inside
      // SnooperNode's resampler becomes an issue: It uses a fixed size buffer
      // of 128 samples, which equates to a much longer duration of audio at the
      // lower sampling rates. With more buffering involved, the output delay
      // must be increased to avoid underruns.
      output_delay_ *= 2;
    } else if (input_params().GetBufferDuration() >
               output_params().GetBufferDuration()) {
      // For the HandlesBackwardsInput() test, the input goes backward by a full
      // buffer. If the duration of an input buffer is larger than an output
      // buffer, this could cause a brief moment of underrun (which is WAI!).
      // Rather than write lots of extra test code around such a specific
      // scenario, just fudge the delay up a little such that the underrun
      // cannot occur.
      output_delay_ += input_params().GetBufferDuration();
    }

    // Determine the maximum allowable error when measuring expected amplitudes.
    // This varies with the sampling rate because the loss of resolution at the
    // lower sampling rates can introduce error in the "amplitude-sensing
    // logic." Higher frequencies in the audio signal are especially vulnerable
    // to the error introduced by using low sampling rates.
    constexpr double kHighResAllowedError = 0.02;  // 2% at 48 kHz
    constexpr double kHighResSampleRate = 48000;
    constexpr double kLowResAllowedError = 0.10;  // 10% at 8 kHz
    constexpr double kLowResSampleRate = 8000;
    const int the_lower_sample_rate =
        std::min(input_params().sample_rate(), output_params().sample_rate());
    const double t = (the_lower_sample_rate - kHighResSampleRate) /
                     (kLowResSampleRate - kHighResSampleRate);
    max_relative_error_ =
        kHighResAllowedError + t * (kLowResAllowedError - kHighResAllowedError);

    // Initialize a test clock and task runner. The starting TimeTicks value is
    // "huge" to ensure time calculations are being tested for overflow cases.
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time(), base::TimeTicks() + base::Microseconds(INT64_C(1) << 62));
  }

  void TearDown() override {
    // If the "dump-as-wav" command-line switch is present, dump whatever has
    // been recorded in the consumer.
    const base::FilePath path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            kDumpAsWavSwitch);
    if (!path.empty()) {
      if (consumer_) {
        consumer_->SaveToFile(path);
      } else {
        LOG(ERROR) << "No consumer: ignoring --" << kDumpAsWavSwitch;
      }
    }
  }

  // Selects which frequency to return for each channel based on the input and
  // output channel layout.
  enum WhichFlow : int8_t {
    FOR_INPUT,
    FOR_SWAPPED_INPUT,
    FOR_OUTPUT,
    FOR_SWAPPED_OUTPUT,
  };

  double GetLeftChannelFrequency(WhichFlow which) const {
    switch (which) {
      case FOR_INPUT:
      case FOR_OUTPUT:
        return kLeftChannelFrequency;
      case FOR_SWAPPED_INPUT:
      case FOR_SWAPPED_OUTPUT:
        return kRightChannelFrequency;
    }
  }

  double GetRightChannelFrequency(WhichFlow which) const {
    switch (which) {
      // If the test parameters call for stereo→mono channel down-mixing, use
      // the left channel frequency again for the right channel input. Down-
      // mixing is tested elsewhere.
      case FOR_INPUT:
        return (output_params().channels() == 1 ? kLeftChannelFrequency
                                                : kRightChannelFrequency);
      case FOR_SWAPPED_INPUT:
        return (output_params().channels() == 1 ? kRightChannelFrequency
                                                : kLeftChannelFrequency);

      // If the input was monaural, the output's right channel should contain
      // the input's "left" channel frequency.
      case FOR_OUTPUT:
        return (input_params().channels() == 1 ? kLeftChannelFrequency
                                               : kRightChannelFrequency);
      case FOR_SWAPPED_OUTPUT:
        return (input_params().channels() == 1 ? kRightChannelFrequency
                                               : kLeftChannelFrequency);
    }
  }

  void CreateNewPipeline() {
    group_member_.emplace(input_params());
    group_member_->SetChannelTone(0, GetLeftChannelFrequency(FOR_INPUT));
    if (input_params().channels() > 1) {
      group_member_->SetChannelTone(1, GetRightChannelFrequency(FOR_INPUT));
    }
    group_member_->SetVolume(kSourceVolume);

    node_.emplace(input_params(), output_params());
    group_member_->StartSnooping(node());

    consumer_.emplace(output_params().channels(),
                      output_params().sample_rate());
  }

  // Have the SnooperNode render more output data and store it in the consumer
  // for later analysis.
  void RenderAndConsume(base::TimeTicks output_time) {
    // Assign invalid sample values to the AudioBus. Then, after the Render()
    // call, confirm that every sample was overwritten in the output AudioBus.
    const auto bus = media::AudioBus::Create(output_params());
    for (int ch = 0; ch < bus->channels(); ++ch) {
      std::fill_n(bus->channel(ch), bus->frames(), kInvalidAudioSample);
    }

    // If the SnooperNode provides a suggestion, check that |output_time| is
    // okay. Otherwise, Render() will be producing zero-fill gaps as the end of
    // |bus|. Don't do this check if there is already a test failure, and this
    // would just keep spamming the test output.
    if (!HasFailure()) {
      const std::optional<base::TimeTicks> suggestion =
          node_->SuggestLatestRenderTime(bus->frames());
      if (suggestion) {
        EXPECT_LE(output_time, *suggestion)
            << "at frame=" << consumer_->GetRecordedFrameCount();
      }
    }

    node_->Render(output_time, bus.get());

    for (int ch = 0; ch < bus->channels(); ++ch) {
      EXPECT_FALSE(
          std::any_of(bus->channel(ch), bus->channel(ch) + bus->frames(),
                      [](float x) { return x == kInvalidAudioSample; }))
          << " at output_time=" << output_time << ", ch=" << ch;
    }
    consumer_->Consume(*bus);
  }

  // Post delayed tasks to schedule normal, uninterrupted input with the default
  // kInputAdvanceTime delay.
  void ScheduleDefaultInputTasks(double skew = 1.0) {
    const base::TimeTicks start_time = task_runner_->NowTicks();
    const base::TimeTicks end_time = start_time + kTestDuration;
    const double time_step = skew / input_params().sample_rate();
    for (int position = 0;; position += input_params().frames_per_buffer()) {
      const base::TimeTicks task_time =
          start_time + base::Seconds(position * time_step);
      if (task_time >= end_time) {
        break;
      }
      const base::TimeTicks reference_time = task_time + kInputAdvanceTime;
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeLoopbackGroupMember::RenderMoreAudio,
                         base::Unretained(group_member()), reference_time),
          task_time - start_time);
    }
  }

  // Post delayed tasks to schedule normal, uninterrupted output rendering to
  // occur at the default kCaptureDelay.
  void ScheduleDefaultRenderTasks(double skew = 1.0) {
    const base::TimeTicks start_time = task_runner_->NowTicks();
    const base::TimeTicks end_time = start_time + kTestDuration;
    const double time_step = skew / output_params().sample_rate();
    for (int position = 0;; position += output_params().frames_per_buffer()) {
      const base::TimeTicks task_time =
          start_time + base::Seconds(position * time_step);
      if (task_time >= end_time) {
        break;
      }
      const base::TimeTicks reference_time = task_time - output_delay();
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&SnooperNodeTest::RenderAndConsume,
                         base::Unretained(this), reference_time),
          task_time - start_time);
    }
  }

  void RunAllPendingTasks() { task_runner_->FastForwardUntilNoTasksRemain(); }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  // A suitable output delay to use for rendering audio from the pipeline. See
  // comments in SetUp() for further details.
  base::TimeDelta output_delay_;

  // The maximum allowable error relative to an expected amplitude.
  double max_relative_error_ = 0.0;

  // The pipeline from source to consumer.
  std::optional<FakeLoopbackGroupMember> group_member_;
  std::optional<SnooperNode> node_;
  std::optional<FakeConsumer> consumer_;
};

// The skew test here is generating 10 seconds of audio per iteration, with
// 5*5=25 iterations. That's 250 seconds of audio being generated to check for
// skew-related issues. That's a lot of processing power needed! Thus, only
// enable this test on optimized, non-debug builds, where it will run in a
// reasonable amount of time. http://crbug.com/842428
#ifdef NDEBUG
#define MAYBE_ContinuousAudioFlowAdaptsToSkew ContinuousAudioFlowAdaptsToSkew
#else
#define MAYBE_ContinuousAudioFlowAdaptsToSkew \
  DISABLED_ContinuousAudioFlowAdaptsToSkew
#endif
// Tests that the internal time-stretching logic can handle various combinations
// of input and output skews.
TEST_P(SnooperNodeTest, MAYBE_ContinuousAudioFlowAdaptsToSkew) {
  // Note: A skew of 0.999 or 1.001 is very extreme. This is like saying the
  // clocks drift 1 ms for every second that goes by. If the implementation can
  // handle that, it's very likely to do a perfect job in-the-wild.
  for (double input_skew = 0.999; input_skew <= 1.001; input_skew += 0.0005) {
    for (double output_skew = 0.999; output_skew <= 1.001;
         output_skew += 0.0005) {
      SCOPED_TRACE(testing::Message() << "input_skew=" << input_skew
                                      << ", output_skew=" << output_skew);

      CreateNewPipeline();
      ScheduleDefaultInputTasks(input_skew);
      ScheduleDefaultRenderTasks(output_skew);
      RunAllPendingTasks();

      // All rendering for points-in-time before the audio from the source was
      // first recorded should be silence.
      const double expected_end_of_silence_position =
          ((input_skew * kInputAdvanceTime.InSecondsF()) +
           (output_skew * output_delay().InSecondsF())) *
          output_params().sample_rate();
      const double frames_in_one_millisecond =
          output_params().sample_rate() /
          double{base::Time::kMillisecondsPerSecond};
      EXPECT_NEAR(expected_end_of_silence_position,
                  consumer()->FindEndOfSilence(0, 0),
                  frames_in_one_millisecond);
      if (output_params().channels() > 1) {
        EXPECT_NEAR(expected_end_of_silence_position,
                    consumer()->FindEndOfSilence(1, 0),
                    frames_in_one_millisecond);
      }

      // Analyze the recording in several places for the expected tones.
      constexpr int kNumToneChecks = 16;
      for (int i = 1; i <= kNumToneChecks; ++i) {
        const int end_frame =
            consumer()->GetRecordedFrameCount() * i / kNumToneChecks;
        SCOPED_TRACE(testing::Message() << "end_frame=" << end_frame);
        EXPECT_NEAR(kSourceVolume,
                    consumer()->ComputeAmplitudeAt(
                        0, GetLeftChannelFrequency(FOR_OUTPUT), end_frame),
                    kSourceVolume * max_relative_error());
        if (output_params().channels() > 1) {
          EXPECT_NEAR(kSourceVolume,
                      consumer()->ComputeAmplitudeAt(
                          1, GetRightChannelFrequency(FOR_OUTPUT), end_frame),
                      kSourceVolume * max_relative_error());
        }
      }

      if (HasFailure()) {
        return;
      }
    }
  }
}

// Tests that gaps in the input are detected, are handled by introducing
// zero-fill gaps in the output, and don't throw-off the timing/synchronization
// between input and output.
TEST_P(SnooperNodeTest, HandlesMissingInput) {
  CreateNewPipeline();

  // Schedule all input tasks, with drops to occur once per second for 1/4
  // second duration.
  const base::TimeTicks start_time = task_runner()->NowTicks();
  const base::TimeTicks end_time = start_time + kTestDuration;
  const double time_step = 1.0 / input_params().sample_rate();
  const int input_frames_in_one_second = input_params().sample_rate();
  // Drop duration: 1/4 second in terms of frames, aligned to frame buffer size.
  const int drop_duration =
      ((input_frames_in_one_second / 4) / input_params().frames_per_buffer()) *
      input_params().frames_per_buffer();
  int next_drop_position = input_frames_in_one_second;
  for (int position = 0;; position += input_params().frames_per_buffer()) {
    if (position >= next_drop_position) {
      position += drop_duration;
      next_drop_position += input_frames_in_one_second;
    }
    const base::TimeTicks task_time =
        start_time + base::Seconds(position * time_step);
    if (task_time >= end_time) {
      break;
    }
    const base::TimeTicks reference_time = task_time + kInputAdvanceTime;
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeLoopbackGroupMember::RenderMoreAudio,
                       base::Unretained(group_member()), reference_time),
        task_time - start_time);
  }

  ScheduleDefaultRenderTasks();
  RunAllPendingTasks();

  // Check that there is silence in the drop positions, and that tones are
  // present around the silent sections. The ranges are adjusted to be 20 ms
  // away from the exact begin/end positions to account for a reasonable amount
  // of variance in due to the input buffer intervals.
  const int output_frames_in_one_second = output_params().sample_rate();
  const int output_frames_in_a_quarter_second = output_frames_in_one_second / 4;
  const int output_frames_in_20_milliseconds =
      output_frames_in_one_second * 20 / base::Time::kMillisecondsPerSecond;
  int output_silence_position =
      ((kInputAdvanceTime + output_delay()).InSecondsF() + 1.0) *
      output_params().sample_rate();
  for (int gap = 0; gap < 5; ++gap) {
    SCOPED_TRACE(testing::Message() << "gap=" << gap);

    // Just before the drop, there should be a tone.
    const int position_a_little_before_silence_begins =
        output_silence_position - output_frames_in_20_milliseconds;
    EXPECT_NEAR(
        kSourceVolume,
        consumer()->ComputeAmplitudeAt(0, GetLeftChannelFrequency(FOR_OUTPUT),
                                       position_a_little_before_silence_begins),
        kSourceVolume * max_relative_error());
    if (output_params().channels() > 1) {
      EXPECT_NEAR(kSourceVolume,
                  consumer()->ComputeAmplitudeAt(
                      1, GetRightChannelFrequency(FOR_OUTPUT),
                      position_a_little_before_silence_begins),
                  kSourceVolume * max_relative_error());
    }

    // There should be silence during the drop.
    const int position_a_little_after_silence_begins =
        output_silence_position + output_frames_in_20_milliseconds;
    const int position_a_little_before_silence_ends =
        position_a_little_after_silence_begins +
        output_frames_in_a_quarter_second -
        2 * output_frames_in_20_milliseconds;
    EXPECT_TRUE(
        consumer()->IsSilentInRange(0, position_a_little_after_silence_begins,
                                    position_a_little_before_silence_ends));

    // Finally, the tone should be back after the drop.
    const int position_a_little_after_silence_ends =
        position_a_little_before_silence_ends +
        2 * output_frames_in_20_milliseconds;
    EXPECT_NEAR(
        kSourceVolume,
        consumer()->ComputeAmplitudeAt(0, GetLeftChannelFrequency(FOR_OUTPUT),
                                       position_a_little_after_silence_ends),
        kSourceVolume * max_relative_error());
    if (output_params().channels() > 1) {
      EXPECT_NEAR(kSourceVolume,
                  consumer()->ComputeAmplitudeAt(
                      1, GetRightChannelFrequency(FOR_OUTPUT),
                      position_a_little_after_silence_ends),
                  kSourceVolume * max_relative_error());
    }
    output_silence_position += output_frames_in_one_second;
  }
}

// Tests that a backwards-jump in input reference timestamps doesn't attempt to
// "re-write history" and otherwise maintains the timing/synchronization between
// input and output. This is a regression test for http://crbug.com/934770.
TEST_P(SnooperNodeTest, HandlesBackwardsInput) {
  CreateNewPipeline();

  // Schedule all input tasks. At the halfway point, simulate a device change
  // that shifts the timestamps backward by one buffer duration, and the
  // left/right sound tones are swapped.
  const base::TimeTicks start_time = task_runner()->NowTicks();
  const base::TimeTicks end_time = start_time + kTestDuration;
  const double time_step = 1.0 / input_params().sample_rate();
  const int change_position =
      input_params().sample_rate() * kTestDuration.InSeconds() / 2;
  int position_offset = 0;
  for (int position = 0;; position += input_params().frames_per_buffer()) {
    const base::TimeTicks task_time =
        start_time + base::Seconds(position * time_step);
    if (task_time >= end_time) {
      break;
    }
    if (position_offset == 0 && position >= change_position) {
      position_offset = -input_params().frames_per_buffer();
      task_runner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](SnooperNodeTest* test) {
                test->group_member()->SetChannelTone(
                    0, test->GetLeftChannelFrequency(FOR_SWAPPED_INPUT));
                if (test->input_params().channels() > 1) {
                  test->group_member()->SetChannelTone(
                      1, test->GetRightChannelFrequency(FOR_SWAPPED_INPUT));
                }
              },
              this),
          task_time - start_time);
    }
    const base::TimeTicks reference_time =
        start_time + kInputAdvanceTime +
        base::Seconds((position + position_offset) * time_step);
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeLoopbackGroupMember::RenderMoreAudio,
                       base::Unretained(group_member()), reference_time),
        task_time - start_time);
  }

  ScheduleDefaultRenderTasks();
  RunAllPendingTasks();

  // In the consumer's recording, there should be audio having the default tones
  // before before the halfway point. After the halfway point, the tones should
  // be swapped (left vs right). Sample once every second, starting at a
  // ~half-second offset.
  const int output_position_halfway =
      (kInputAdvanceTime + output_delay() + (kTestDuration / 2)).InSecondsF() *
      output_params().sample_rate();
  const int output_frames_in_one_second = output_params().sample_rate();
  int output_position =
      ((kInputAdvanceTime + output_delay()).InSecondsF() + 0.5) *
      output_params().sample_rate();
  for (int output_end = consumer()->GetRecordedFrameCount();
       output_position < output_end;
       output_position += output_frames_in_one_second) {
    const int left_ch_freq = (output_position < output_position_halfway)
                                 ? GetLeftChannelFrequency(FOR_OUTPUT)
                                 : GetLeftChannelFrequency(FOR_SWAPPED_OUTPUT);
    EXPECT_NEAR(
        kSourceVolume,
        consumer()->ComputeAmplitudeAt(0, left_ch_freq, output_position),
        kSourceVolume * max_relative_error());
    if (output_params().channels() > 1) {
      const int right_ch_freq =
          (output_position < output_position_halfway)
              ? GetRightChannelFrequency(FOR_OUTPUT)
              : GetRightChannelFrequency(FOR_SWAPPED_OUTPUT);
      EXPECT_NEAR(
          kSourceVolume,
          consumer()->ComputeAmplitudeAt(1, right_ch_freq, output_position),
          kSourceVolume * max_relative_error());
    }
  }
}

// Tests that reasonable render times are suggested as audio is feeding into, or
// not feeding into, the SnooperNode.
TEST_P(SnooperNodeTest, SuggestsRenderTimes) {
  constexpr base::TimeDelta kTwentyMilliseconds = base::Milliseconds(20);

  CreateNewPipeline();

  // Before any audio has flowed into the SnooperNode, there should be nothing
  // to base a suggestion on.
  EXPECT_FALSE(
      node()->SuggestLatestRenderTime(output_params().frames_per_buffer()));

  // Feed-in the first buffer and expect a render time suggestion that is
  // greater than 150% the output buffer's duration amount of time in the
  // past. (The extra 50% is a safety margin; see internal code comments for
  // further details.) The suggestion should also not be too far in the past.
  const base::TimeTicks first_input_time = task_runner()->NowTicks();
  group_member()->RenderMoreAudio(first_input_time);
  const std::optional<base::TimeTicks> first_suggestion =
      node()->SuggestLatestRenderTime(output_params().frames_per_buffer());
  ASSERT_TRUE(first_suggestion);
  base::TimeTicks time_at_end_of_input =
      first_input_time + input_params().GetBufferDuration();
  const base::TimeDelta required_duration_buffered =
      output_params().GetBufferDuration() * 3 / 2;
  EXPECT_GT(time_at_end_of_input - required_duration_buffered,
            *first_suggestion);
  EXPECT_LT(
      time_at_end_of_input - required_duration_buffered - kTwentyMilliseconds,
      *first_suggestion);

  // If another suggestion is solicited before more input was provided,
  // SnooperNode shouldn't give one.
  for (int i = 0; i < 3; ++i) {
    EXPECT_FALSE(
        node()->SuggestLatestRenderTime(output_params().frames_per_buffer()));
  }

  // When feeding-in successive buffers, a new suggestion can be given after
  // each, reflecting the timing of the additional audio that has been buffered.
  for (int i = 1; i <= 3; ++i) {
    const base::TimeTicks next_input_time =
        first_input_time +
        base::Seconds(i * input_params().frames_per_buffer() /
                      static_cast<double>(input_params().sample_rate()));
    group_member()->RenderMoreAudio(next_input_time);
    const std::optional<base::TimeTicks> next_suggestion =
        node()->SuggestLatestRenderTime(output_params().frames_per_buffer());
    ASSERT_TRUE(next_suggestion);
    time_at_end_of_input = next_input_time + input_params().GetBufferDuration();
    EXPECT_GT(time_at_end_of_input - required_duration_buffered,
              *next_suggestion);
    EXPECT_LT(
        time_at_end_of_input - required_duration_buffered - kTwentyMilliseconds,
        *next_suggestion);
  }
}

namespace {

// Used in the HandlesSeekedRenderTimes test below. Returns one of 10 possible
// tone frequencies to use at the specified time |offset| in the audio.
double MapTimeOffsetToATone(base::TimeDelta offset) {
  constexpr double kMinFrequency = 200;
  constexpr double kMaxFrequency = 2000;
  constexpr int kNumToneSteps = 10;

  const int64_t step_number = offset.IntDiv(kTestDuration / kNumToneSteps);
  const double t = static_cast<double>(step_number) / kNumToneSteps;
  return kMinFrequency + t * (kMaxFrequency - kMinFrequency);
}

}  // namespace

// Tests that the SnooperNode can be asked to seek (forward or backward) its
// Render() positions, as the needs of the system demand.
TEST_P(SnooperNodeTest, HandlesSeekedRenderTimes) {
  constexpr base::TimeDelta kQuarterSecond = base::Milliseconds(250);

  CreateNewPipeline();

  // Schedule input tasks where the audio tones are changed once per second, to
  // allow for identifying the timing of the audio in the consumer's recording
  // later on.
  const base::TimeTicks start_time = task_runner()->NowTicks();
  const base::TimeTicks end_time = start_time + kTestDuration;
  double time_step = 1.0 / input_params().sample_rate();
  for (int position = 0;; position += input_params().frames_per_buffer()) {
    const base::TimeTicks task_time =
        start_time + base::Seconds(position * time_step);
    if (task_time >= end_time) {
      break;
    }
    const base::TimeTicks reference_time = task_time + kInputAdvanceTime;
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](FakeLoopbackGroupMember* group_member,
               base::TimeTicks start_time, base::TimeTicks reference_time) {
              group_member->SetChannelTone(
                  FakeLoopbackGroupMember::kSetAllChannels,
                  MapTimeOffsetToATone(reference_time - start_time));
              group_member->RenderMoreAudio(reference_time);
            },
            group_member(), start_time, reference_time),
        task_time - start_time);
  }

  // Schedule normal render tasks for the first third of the test, then skip
  // back a quarter-second and run for another third of the test, then skip
  // forward a quarter-second and run to the end.
  time_step = 1.0 / output_params().sample_rate();
  for (int position = 0;; position += output_params().frames_per_buffer()) {
    const base::TimeTicks task_time =
        start_time + base::Seconds(position * time_step);
    if (task_time >= end_time) {
      break;
    }
    base::TimeDelta time_offset = task_time - start_time;
    if (time_offset < (kTestDuration / 3) ||
        time_offset >= (kTestDuration * 2 / 3)) {
      time_offset = base::TimeDelta();
    } else {
      time_offset = -kQuarterSecond;
    }
    const base::TimeTicks reference_time =
        task_time + time_offset - output_delay();
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SnooperNodeTest::RenderAndConsume,
                       base::Unretained(this), reference_time),
        task_time - start_time);
  }

  RunAllPendingTasks();

  // Examine the consumer's recorded audio for the expected audio signal: For
  // the first third of the test, the consumer should hear the first few tones.
  // Then, at the point where rendering seeked backward, there will be a
  // zero-fill gap for a quarter second, followed by the tone changes being
  // "late" by a quarter second. Finally, at the point where rendering seeked
  // forward, the tone changes will be shifted back again.
  const base::TimeDelta lead_in = kInputAdvanceTime + output_delay();
  for (base::TimeDelta recording_time = lead_in + kQuarterSecond;
       recording_time < kTestDuration; recording_time += kQuarterSecond) {
    const base::TimeDelta render_time = recording_time - kInputAdvanceTime;

    base::TimeDelta input_time =
        recording_time - lead_in - input_params().GetBufferDuration();
    // The recording is shifted forward during the middle-third of the test.
    if (render_time >= (kTestDuration / 3) &&
        render_time < (kTestDuration * 2 / 3)) {
      input_time -= kQuarterSecond;
    }

    const double expected_freq = MapTimeOffsetToATone(input_time);
    SCOPED_TRACE(testing::Message() << "recording_time=" << recording_time
                                    << ", expected_freq=" << expected_freq);
    const int position =
        output_params().sample_rate() * recording_time.InSecondsF() -
        output_params().frames_per_buffer();
    if (render_time >= (kTestDuration / 3) &&
        render_time < (kTestDuration / 3 + kQuarterSecond)) {
      // Special case: Expect the zero-fill gap immediately after the first
      // discontinuity.
      for (int ch = 0; ch < output_params().channels(); ++ch) {
        EXPECT_TRUE(consumer()->IsSilentInRange(
            ch, position - output_params().frames_per_buffer(), position));
      }
    } else {
      for (int ch = 0; ch < output_params().channels(); ++ch) {
        EXPECT_NEAR(kSourceVolume,
                    consumer()->ComputeAmplitudeAt(ch, expected_freq, position),
                    kSourceVolume * max_relative_error());
      }
    }
  }
}

InputAndOutputParams MakeParams(
    media::ChannelLayoutConfig input_channel_layout_config,
    int input_sample_rate,
    int input_frames_per_buffer,
    media::ChannelLayoutConfig output_channel_layout_config,
    int output_sample_rate,
    int output_frames_per_buffer) {
  return InputAndOutputParams{
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             input_channel_layout_config, input_sample_rate,
                             input_frames_per_buffer),
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             output_channel_layout_config, output_sample_rate,
                             output_frames_per_buffer)};
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SnooperNodeTest,
    testing::Values(MakeParams(media::ChannelLayoutConfig::Stereo(),
                               48000,
                               480,
                               media::ChannelLayoutConfig::Stereo(),
                               48000,
                               480),
                    MakeParams(media::ChannelLayoutConfig::Stereo(),
                               48000,
                               64,
                               media::ChannelLayoutConfig::Stereo(),
                               48000,
                               480),
                    MakeParams(media::ChannelLayoutConfig::Stereo(),
                               44100,
                               64,
                               media::ChannelLayoutConfig::Stereo(),
                               48000,
                               480),
                    MakeParams(media::ChannelLayoutConfig::Stereo(),
                               48000,
                               512,
                               media::ChannelLayoutConfig::Stereo(),
                               44100,
                               441),
                    MakeParams(media::ChannelLayoutConfig::Mono(),
                               8000,
                               64,
                               media::ChannelLayoutConfig::Stereo(),
                               48000,
                               480),
                    MakeParams(media::ChannelLayoutConfig::Stereo(),
                               48000,
                               480,
                               media::ChannelLayoutConfig::Mono(),
                               8000,
                               80)));

}  // namespace
}  // namespace audio
