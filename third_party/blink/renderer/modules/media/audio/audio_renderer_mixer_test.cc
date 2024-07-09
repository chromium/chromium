// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <tuple>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_input.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_pool.h"

namespace blink {

// Parameters which control the many input case tests.
constexpr int kMixerInputs = 8;
constexpr int kOddMixerInputs = 7;
constexpr int kMixerCycles = 3;

// Parameters used for testing.
constexpr media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
constexpr int kHighLatencyBufferSize = 8192;
constexpr int kLowLatencyBufferSize = 256;

// Number of full sine wave cycles for each Render() call.
constexpr int kSineCycles = 4;

// Input sample frequencies for testing.
constexpr int kTestInputLower = 44100;
constexpr int kTestInputHigher = 48000;
constexpr int kTestInput3Rates[] = {22050, 44100, 48000};

// Tuple of <input sampling rates, number of input sample rates,
// output sampling rate, epsilon>.
using AudioRendererMixerTestData =
    std::tuple<const int* const, size_t, int, double>;

class AudioRendererMixerTest
    : public testing::TestWithParam<AudioRendererMixerTestData>,
      public AudioRendererMixerPool {
 public:
  AudioRendererMixerTest()
      : epsilon_(std::get<3>(GetParam())), half_fill_(false) {
    // Create input parameters based on test parameters.
    const int* const sample_rates = std::get<0>(GetParam());
    size_t sample_rates_count = std::get<1>(GetParam());
    for (size_t i = 0; i < sample_rates_count; ++i) {
      input_parameters_.emplace_back(
          media::AudioParameters::AUDIO_PCM_LINEAR,
          media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
          sample_rates[i], kHighLatencyBufferSize);
    }

    // Create output parameters based on test parameters.
    output_parameters_ = media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
        std::get<2>(GetParam()), kLowLatencyBufferSize);

    sink_ = base::MakeRefCounted<media::MockAudioRendererSink>();
    EXPECT_CALL(*sink_.get(), Start());
    EXPECT_CALL(*sink_.get(), Stop());

    mixer_ = std::make_unique<AudioRendererMixer>(output_parameters_, sink_);
    mixer_callback_ = sink_->callback();

    audio_bus_ = media::AudioBus::Create(output_parameters_);
    expected_audio_bus_ = media::AudioBus::Create(output_parameters_);

    // Allocate one callback for generating expected results.
    double step = kSineCycles /
                  static_cast<double>(output_parameters_.frames_per_buffer());
    expected_callback_ = std::make_unique<media::FakeAudioRenderCallback>(
        step, output_parameters_.sample_rate());

    expected_callback_->set_needs_fade_in(true);
  }

  AudioRendererMixerTest(const AudioRendererMixerTest&) = delete;
  AudioRendererMixerTest& operator=(const AudioRendererMixerTest&) = delete;

  AudioRendererMixer* GetMixer(const FrameToken&,
                               const media::AudioParameters&,
                               media::AudioLatency::Type,
                               const media::OutputDeviceInfo&,
                               scoped_refptr<media::AudioRendererSink>) final {
    return mixer_.get();
  }

  void ReturnMixer(AudioRendererMixer* mixer) override {
    EXPECT_EQ(mixer_.get(), mixer);
  }

  scoped_refptr<media::AudioRendererSink> GetSink(const LocalFrameToken&,
                                                  std::string_view) override {
    return sink_;
  }

  void InitializeInputs(int inputs_per_sample_rate) {
    mixer_inputs_.reserve(inputs_per_sample_rate * input_parameters_.size());
    fake_callbacks_.reserve(inputs_per_sample_rate * input_parameters_.size());

    for (size_t i = 0, input = 0; i < input_parameters_.size(); ++i) {
      // Setup FakeAudioRenderCallback step to compensate for resampling.
      double scale_factor =
          input_parameters_[i].sample_rate() /
          static_cast<double>(output_parameters_.sample_rate());
      double step =
          kSineCycles /
          (scale_factor *
           static_cast<double>(output_parameters_.frames_per_buffer()));

      for (int j = 0; j < inputs_per_sample_rate; ++j, ++input) {
        fake_callbacks_.push_back(
            std::make_unique<media::FakeAudioRenderCallback>(
                step, output_parameters_.sample_rate()));
        mixer_inputs_.push_back(CreateMixerInput());
        mixer_inputs_[input]->Initialize(input_parameters_[i],
                                         fake_callbacks_[input].get());
        mixer_inputs_[input]->SetVolume(1.0f);
      }
    }
  }

  bool ValidateAudioData(int index, int frames, float scale, double epsilon) {
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      for (int j = index; j < frames; j++) {
        double error = fabs(audio_bus_->channel(i)[j] -
                            expected_audio_bus_->channel(i)[j] * scale);
        // The second comparison is for the case when scale is set to 0
        // (and less that 1 in general)
        if ((error > epsilon * scale) && (error > epsilon)) {
          EXPECT_NEAR(expected_audio_bus_->channel(i)[j] * scale,
                      audio_bus_->channel(i)[j], epsilon * scale)
              << " i=" << i << ", j=" << j;
          return false;
        }
      }
    }
    return true;
  }

  bool ValidateAudioData(int index, int frames, float scale) {
    return ValidateAudioData(index, frames, scale, epsilon_);
  }

  bool RenderAndValidateAudioData(float scale) {
    if (half_fill_) {
      for (size_t i = 0; i < fake_callbacks_.size(); ++i) {
        fake_callbacks_[i]->set_half_fill(true);
      }
      expected_callback_->set_half_fill(true);
      // Initialize the AudioBus completely or we'll run into Valgrind problems
      // during the verification step below.
      expected_audio_bus_->Zero();
    }

    // Render actual audio data.
    int frames = mixer_callback_->Render(
        base::TimeDelta(), base::TimeTicks::Now(), {}, audio_bus_.get());
    if (frames != audio_bus_->frames()) {
      return false;
    }

    // Render expected audio data (without scaling).
    expected_callback_->Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                               expected_audio_bus_.get());

    if (half_fill_) {
      // In this case, just verify that every frame was initialized, this will
      // only fail under tooling such as valgrind.
      return ValidateAudioData(0, frames, 0,
                               std::numeric_limits<double>::max());
    } else {
      return ValidateAudioData(0, frames, scale);
    }
  }

  // Fill |audio_bus_| fully with |value|.
  void FillAudioData(float value) {
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      std::fill(audio_bus_->channel(i),
                audio_bus_->channel(i) + audio_bus_->frames(), value);
    }
  }

  // Verify silence when mixer inputs are in pre-Start() and post-Start().
  void StartTest(int inputs) {
    InitializeInputs(inputs);

    // Verify silence before any inputs have been started.  Fill the buffer
    // before hand with non-zero data to ensure we get zeros back.
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    // Start() all even numbered mixer inputs and ensure we still get silence.
    for (size_t i = 0; i < mixer_inputs_.size(); i += 2) {
      mixer_inputs_[i]->Start();
    }
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    // Start() all mixer inputs and ensure we still get silence.
    for (size_t i = 1; i < mixer_inputs_.size(); i += 2) {
      mixer_inputs_[i]->Start();
    }
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Stop();
    }
  }

  // Verify output when mixer inputs are in post-Play() state.
  void PlayTest(int inputs) {
    InitializeInputs(inputs);

    // Play() all mixer inputs and ensure we get the right values.
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    for (int i = 0; i < kMixerCycles; ++i) {
      ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size()));
    }

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Stop();
    }
  }

  // Verify volume adjusted output when mixer inputs are in post-Play() state.
  void PlayVolumeAdjustedTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Set a different volume for each mixer input and verify the results.
    float total_scale = 0;
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      float volume = static_cast<float>(i) / mixer_inputs_.size();
      total_scale += volume;
      EXPECT_TRUE(mixer_inputs_[i]->SetVolume(volume));
    }
    for (int i = 0; i < kMixerCycles; ++i) {
      ASSERT_TRUE(RenderAndValidateAudioData(total_scale));
    }

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Stop();
    }
  }

  // Verify output when mixer inputs can only partially fulfill a Render().
  void PlayPartialRenderTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Verify a properly filled buffer when half filled (remainder zeroed).
    half_fill_ = true;
    ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size()));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Stop();
    }
  }

  // Verify output when mixer inputs are in Pause() state.
  void PauseTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Pause() all even numbered mixer inputs and ensure we get the right value.
    for (size_t i = 0; i < mixer_inputs_.size(); i += 2) {
      mixer_inputs_[i]->Pause();
    }
    for (int i = 0; i < kMixerCycles; ++i) {
      ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size() / 2));
    }

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Stop();
    }
  }

  // Verify output when mixer inputs are in post-Stop() state.
  void StopTest(int inputs) {
    InitializeInputs(inputs);

    // Start() and Stop() all inputs.
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Stop();
    }

    // Verify we get silence back; fill |audio_bus_| before hand to be sure.
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));
  }

  // Verify output when mixer inputs in mixed post-Stop() and post-Play()
  // states.
  void MixedStopPlayTest(int inputs) {
    InitializeInputs(inputs);

    // Start() all inputs.
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
    }

    // Stop() all even numbered mixer inputs and Play() all odd numbered inputs
    // and ensure we get the right value.
    for (size_t i = 1; i < mixer_inputs_.size(); i += 2) {
      mixer_inputs_[i - 1]->Stop();
      mixer_inputs_[i]->Play();
    }

    // Stop the last input in case the number of inputs is odd
    if (mixer_inputs_.size() % 2) {
      mixer_inputs_.back()->Stop();
    }

    ASSERT_TRUE(RenderAndValidateAudioData(
        std::max(1.f, static_cast<float>(floor(mixer_inputs_.size() / 2.f)))));

    for (size_t i = 1; i < mixer_inputs_.size(); i += 2) {
      mixer_inputs_[i]->Stop();
    }
  }

  // Verify that glitch info is being propagated properly.
  void GlitchInfoTest(int inputs) {
    InitializeInputs(inputs);

    // Play() all mixer inputs and ensure we get the right values.
    for (auto& mixer_input : mixer_inputs_) {
      mixer_input->Start();
      mixer_input->Play();
    }

    media::AudioGlitchInfo glitch_info{.duration = base::Milliseconds(100),
                                       .count = 123};
    media::AudioGlitchInfo expected_glitch_info;

    for (int i = 0; i < kMixerCycles; ++i) {
      expected_glitch_info += glitch_info;
      mixer_callback_->Render(base::TimeDelta(), base::TimeTicks::Now(),
                              glitch_info, audio_bus_.get());
    }

    // If the output buffer duration is not divisible by all the input buffer
    // durations, all glitch info will not necessarily have been propagated yet.
    // We call Render with empty glitch info a few more times to flush out any
    // remaining glitch info.
    for (int i = 0; i < kMixerCycles; ++i) {
      mixer_callback_->Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                              audio_bus_.get());
    }

    for (auto& callback : fake_callbacks_) {
      EXPECT_EQ(callback->cumulative_glitch_info(), expected_glitch_info);
    }

    for (auto& mixer_input : mixer_inputs_) {
      mixer_input->Stop();
    }
  }

  scoped_refptr<AudioRendererMixerInput> CreateMixerInput() {
    auto input = base::MakeRefCounted<AudioRendererMixerInput>(
        this, LocalFrameToken(), FrameToken(),
        // default device ID.
        std::string(), media::AudioLatency::Type::kPlayback);
    input->GetOutputDeviceInfoAsync(
        base::DoNothing());  // Primes input, needed for tests.
    task_env_.RunUntilIdle();
    return input;
  }

 protected:
  virtual ~AudioRendererMixerTest() = default;

  base::test::TaskEnvironment task_env_;
  scoped_refptr<media::MockAudioRendererSink> sink_;
  std::unique_ptr<AudioRendererMixer> mixer_;
  raw_ptr<media::AudioRendererSink::RenderCallback> mixer_callback_;
  std::vector<media::AudioParameters> input_parameters_;
  media::AudioParameters output_parameters_;
  std::unique_ptr<media::AudioBus> audio_bus_;
  std::unique_ptr<media::AudioBus> expected_audio_bus_;
  std::vector<scoped_refptr<AudioRendererMixerInput>> mixer_inputs_;
  std::vector<std::unique_ptr<media::FakeAudioRenderCallback>> fake_callbacks_;
  std::unique_ptr<media::FakeAudioRenderCallback> expected_callback_;
  double epsilon_;
  bool half_fill_;
};

class AudioRendererMixerBehavioralTest : public AudioRendererMixerTest {};

ACTION_P(SignalEvent, event) {
  event->Signal();
}

// Verify a mixer with no inputs returns silence for all requested frames.
TEST_P(AudioRendererMixerTest, NoInputs) {
  FillAudioData(1.0f);
  EXPECT_TRUE(RenderAndValidateAudioData(0.0f));
}

// Test mixer output with one input in the pre-Start() and post-Start() state.
TEST_P(AudioRendererMixerTest, OneInputStart) {
  StartTest(1);
}

// Test mixer output with many inputs in the pre-Start() and post-Start() state.
TEST_P(AudioRendererMixerTest, ManyInputStart) {
  StartTest(kMixerInputs);
}

// Test mixer output with one input in the post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlay) {
  PlayTest(1);
}

// Test mixer output with many inputs in the post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlay) {
  PlayTest(kMixerInputs);
}

// Test volume adjusted mixer output with one input in the post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlayVolumeAdjusted) {
  PlayVolumeAdjustedTest(1);
}

// Test volume adjusted mixer output with many inputs in the post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlayVolumeAdjusted) {
  PlayVolumeAdjustedTest(kMixerInputs);
}

// Test mixer output with one input and partial Render() in post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlayPartialRender) {
  PlayPartialRenderTest(1);
}

// Test mixer output with many inputs and partial Render() in post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlayPartialRender) {
  PlayPartialRenderTest(kMixerInputs);
}

// Test mixer output with one input in the post-Pause() state.
TEST_P(AudioRendererMixerTest, OneInputPause) {
  PauseTest(1);
}

// Test mixer output with many inputs in the post-Pause() state.
TEST_P(AudioRendererMixerTest, ManyInputPause) {
  PauseTest(kMixerInputs);
}

// Test mixer output with one input in the post-Stop() state.
TEST_P(AudioRendererMixerTest, OneInputStop) {
  StopTest(1);
}

// Test mixer output with many inputs in the post-Stop() state.
TEST_P(AudioRendererMixerTest, ManyInputStop) {
  StopTest(kMixerInputs);
}

// Test mixer with many inputs in mixed post-Stop() and post-Play() states.
TEST_P(AudioRendererMixerTest, ManyInputMixedStopPlay) {
  MixedStopPlayTest(kMixerInputs);
}

// Test mixer with many inputs in mixed post-Stop() and post-Play() states.
TEST_P(AudioRendererMixerTest, ManyInputMixedStopPlayOdd) {
  // Odd number of inputs per sample rate, to stop them unevenly.
  MixedStopPlayTest(kOddMixerInputs);
}

// Check that AudioGlitchInfo is propagated.
TEST_P(AudioRendererMixerTest, PropagatesAudioGlitchInfo) {
  GlitchInfoTest(kMixerInputs);
}

TEST_P(AudioRendererMixerBehavioralTest, OnRenderError) {
  InitializeInputs(kMixerInputs);
  for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
    mixer_inputs_[i]->Start();
    mixer_inputs_[i]->Play();
    EXPECT_CALL(*fake_callbacks_[i], OnRenderError()).Times(1);
  }

  EXPECT_FALSE(mixer_->HasSinkError());

  mixer_callback_->OnRenderError();
  for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
    mixer_inputs_[i]->Stop();
  }

  EXPECT_TRUE(mixer_->HasSinkError());
}

TEST_P(AudioRendererMixerBehavioralTest, OnRenderErrorPausedInput) {
  InitializeInputs(kMixerInputs);

  for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
    mixer_inputs_[i]->Start();
    EXPECT_CALL(*fake_callbacks_[i], OnRenderError()).Times(1);
  }

  // Fire the error before attaching any inputs.  Ensure an error is recieved
  // even if the input is not connected.
  mixer_callback_->OnRenderError();

  for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
    mixer_inputs_[i]->Stop();
  }
}

// Ensure the physical stream is paused after a certain amount of time with no
// inputs playing.  The test will hang if the behavior is incorrect.
TEST_P(AudioRendererMixerBehavioralTest, MixerPausesStream) {
  const base::TimeDelta kPauseTime = base::Milliseconds(500);
  // This value can't be too low or valgrind, tsan will timeout on the bots.
  const base::TimeDelta kTestTimeout = 10 * kPauseTime;
  mixer_->SetPauseDelayForTesting(kPauseTime);

  base::WaitableEvent pause_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  EXPECT_CALL(*sink_.get(), Pause())
      .Times(2)
      .WillRepeatedly(SignalEvent(&pause_event));
  InitializeInputs(1);

  // Ensure never playing the input results in a sink pause.
  const base::TimeDelta kSleepTime = base::Milliseconds(100);
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (!pause_event.IsSignaled()) {
    mixer_callback_->Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                            audio_bus_.get());
    base::PlatformThread::Sleep(kSleepTime);
    ASSERT_TRUE(base::TimeTicks::Now() - start_time < kTestTimeout);
  }
  pause_event.Reset();

  // Playing the input for the first time should cause a sink play.
  mixer_inputs_[0]->Start();
  EXPECT_CALL(*sink_.get(), Play());
  mixer_inputs_[0]->Play();
  mixer_inputs_[0]->Pause();

  // Ensure once the input is paused the sink eventually pauses.
  start_time = base::TimeTicks::Now();
  while (!pause_event.IsSignaled()) {
    mixer_callback_->Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                            audio_bus_.get());
    base::PlatformThread::Sleep(kSleepTime);
    ASSERT_TRUE(base::TimeTicks::Now() - start_time < kTestTimeout);
  }

  mixer_inputs_[0]->Stop();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AudioRendererMixerTest,
    testing::Values(
        // No resampling, 1 input sample rate.
        std::make_tuple(&kTestInputLower, 1, kTestInputLower, 0.00000048),

        // Upsampling, 1 input sample rate.
        std::make_tuple(&kTestInputLower, 1, kTestInputHigher, 0.01),

        // Downsampling, 1 input sample rate.
        std::make_tuple(&kTestInputHigher, 1, kTestInputLower, 0.01),

        // Downsampling, multuple input sample rates.
        std::make_tuple(static_cast<const int* const>(kTestInput3Rates),
                        std::size(kTestInput3Rates),
                        kTestInput3Rates[0],
                        0.01),

        // Upsampling, multiple sinput sample rates.
        std::make_tuple(static_cast<const int* const>(kTestInput3Rates),
                        std::size(kTestInput3Rates),
                        kTestInput3Rates[2],
                        0.01),

        // Both downsampling and upsampling, multiple input sample rates
        std::make_tuple(static_cast<const int* const>(kTestInput3Rates),
                        std::size(kTestInput3Rates),
                        kTestInput3Rates[1],
                        0.01)));

// Test cases for behavior which is independent of parameters.  Values() doesn't
// support single item lists and we don't want these test cases to run for every
// parameter set.
INSTANTIATE_TEST_SUITE_P(
    All,
    AudioRendererMixerBehavioralTest,
    testing::ValuesIn(std::vector<AudioRendererMixerTestData>(
        1,
        std::make_tuple(&kTestInputLower, 1, kTestInputLower, 0.00000048))));
}  // namespace blink
