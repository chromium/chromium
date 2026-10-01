// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_processor_handler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/audio/audio_debug_recording_helper.h"
#include "media/audio/mock_audio_debug_recording_manager.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "media/webrtc/ml_model_handle.h"
#include "media/webrtc/voice_isolation/mock_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/audio/ml_model_manager.h"
#include "services/audio/voice_isolation_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

using ::testing::_;
using ::testing::Eq;

namespace audio {

namespace {
constexpr int kSampleRate = 48000;
// WebRTC APM requires 10ms buffer size.
constexpr int kFramesPerBuffer = kSampleRate / 100;
}  // namespace

class AudioProcessorHandlerTest : public ::testing::Test {
 protected:
  AudioProcessorHandlerTest() {
    input_params_ = media::AudioParameters(
        media::AudioParameters::Format::AUDIO_PCM_LINEAR,
        media::ChannelLayoutConfig::Mono(), kSampleRate, kFramesPerBuffer);
    output_params_ = input_params_;
  }

  base::test::TaskEnvironment task_environment_;

  media::AudioParameters input_params_;
  media::AudioParameters output_params_;

  base::MockCallback<AudioProcessorHandler::LogCallback> log_callback_;
  base::MockCallback<AudioProcessorHandler::DeliverProcessedAudioCallback>
      deliver_callback_;
  base::MockCallback<AudioProcessorHandler::VolumeAdjustmentCallback>
      volume_callback_;
  base::MockCallback<AudioProcessorHandler::ReferenceStreamErrorCallback>
      error_callback_;

  bool HasVoiceIsolationHandler(const AudioProcessorHandler& handler) {
    return handler.voice_isolation_handler_ != nullptr;
  }

  bool HasProcessingFifo(const AudioProcessorHandler& handler) {
    return handler.processing_fifo_ != nullptr;
  }

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  bool VoiceIsolationHasProcessingThread(const AudioProcessorHandler& handler) {
    return handler.voice_isolation_handler_ &&
           handler.voice_isolation_handler_->HasProcessingThread();
  }

  int GetVoiceIsolationFifoSize(const AudioProcessorHandler& handler) {
    return handler.voice_isolation_handler_
               ? handler.voice_isolation_handler_->GetFifoSizeForTesting()
               : 0;
  }

  static std::unique_ptr<VoiceIsolationHandler>
  CreateVoiceIsolationHandlerWithMock(
      std::unique_ptr<media::MockVoiceIsolation> mock_voice_isolation,
      const media::AudioParameters& output_params,
      VoiceIsolationHandler::DeliverProcessedAudioCallback callback,
      std::unique_ptr<media::AudioDebugRecorder> debug_recorder = nullptr) {
    // Pass-through.
    ON_CALL(*mock_voice_isolation, ProcessAudio(_, _))
        .WillByDefault([](const media::AudioBus& input,
                          media::AudioBus& output) { input.CopyTo(&output); });
    return VoiceIsolationHandler::CreateForTesting(
        std::move(mock_voice_isolation), output_params, callback,
        /*log_callback=*/base::DoNothing(), std::move(debug_recorder));
  }
#endif
};

namespace {

class MockAudioDebugRecorder : public media::AudioDebugRecorder {
 public:
  ~MockAudioDebugRecorder() override = default;
  MOCK_METHOD(void, OnData, (const media::AudioBus* source), (override));
};

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
// Constant levels used to tell the microphone signal apart from the voice
// isolation output in the transition (fade) tests.
constexpr float kMicLevel = 1.0f;
constexpr float kDenoisedLevel = -1.0f;
constexpr float kFadeTolerance = 1e-6f;
// The first fade-in gain is sin^2(pi / (2 * kFramesPerBuffer)) ~= 1.07e-5
// rather than exactly 0.0, so edge samples are checked with a looser tolerance.
constexpr float kFadeEdgeTolerance = 1e-4f;
// Index of the mid-frame sample, where the fade-in gain is sin^2(pi / 4) = 0.5.
constexpr int kMidFrameIndex = kFramesPerBuffer / 2 - 1;

// Fills every channel of `bus` with `value`.
void FillBus(media::AudioBus& bus, float value) {
  for (int ch = 0; ch < bus.channels(); ++ch) {
    std::ranges::fill(bus.channel(ch), value);
  }
}

// Reference rising half-Hann fade-in gain: sin^2(pi * (i + 1) / (2 * N)).
float ExpectedFadeIn(int index, int num_frames) {
  const float sine =
      std::sin(std::numbers::pi_v<float> * static_cast<float>(index + 1) /
               static_cast<float>(2 * num_frames));
  return sine * sine;
}

// Matches VoiceIsolationStartupResult in enums.xml.
enum class VoiceIsolationStartupResult {
  kSuccess = 0,
  kFailed = 1,
  kAborted = 2,
};

class FakeMlModelHandle : public media::MlModelHandle {
 public:
  explicit FakeMlModelHandle(
      base::OnceClosure on_destroy = base::NullCallback())
      : on_destroy_(std::move(on_destroy)),
        reply_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        model_(media::LoadVoiceIsolationTestModel()) {}

  const tflite::FlatBufferModel& Get() override { return *model_; }

 private:
  ~FakeMlModelHandle() override {
    if (on_destroy_) {
      reply_runner_->PostTask(FROM_HERE, std::move(on_destroy_));
    }
  }

  base::OnceClosure on_destroy_;
  scoped_refptr<base::SequencedTaskRunner> reply_runner_;
  std::vector<uint8_t> buffer_;
  std::unique_ptr<tflite::FlatBufferModel> model_;
};

class MockMlModelManager : public MlModelManager {
 public:
  MockMlModelManager() {
    ON_CALL(*this, GetModel(testing::_)).WillByDefault([](mojom::MlModelType) {
      return base::MakeRefCounted<FakeMlModelHandle>();
    });
  }
  MOCK_METHOD(scoped_refptr<media::MlModelHandle>,
              GetModel,
              (mojom::MlModelType model_type),
              (override));
};
#endif

TEST_F(AudioProcessorHandlerTest, ProcessingWithoutVoiceIsolationHandler) {
  media::AudioProcessingSettings settings;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation_handler=*/nullptr);

  handler->StartProcessing();
  EXPECT_FALSE(HasVoiceIsolationHandler(*handler));

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(processed_bus.channels(), output_params_.channels());
        EXPECT_EQ(processed_bus.frames(), output_params_.frames_per_buffer());
        run_loop.Quit();
      });

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                media::AudioGlitchInfo());
  run_loop.Run();

  handler->StopProcessing();
}

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
TEST_F(AudioProcessorHandlerTest, ProcessingWithVoiceIsolationHandler) {
  media::AudioProcessingSettings settings;
  settings.voice_isolation = true;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr =
      mock_voice_isolation.get();

  // Audio is flowing through voice isolation processing.
  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _))
      .WillOnce([](const media::AudioBus& input, media::AudioBus& output) {
        input.CopyTo(&output);
      });

  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get()));

  handler->StartProcessing();
  EXPECT_TRUE(HasVoiceIsolationHandler(*handler));

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  // Processed audio is delivered.
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(processed_bus.channels(), output_params_.channels());
        EXPECT_EQ(processed_bus.frames(), output_params_.frames_per_buffer());
        run_loop.Quit();
      });

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                media::AudioGlitchInfo());
  run_loop.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest, VolumeAdjustmentWithVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.automatic_gain_control = true;
  settings.voice_isolation = true;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr =
      mock_voice_isolation.get();

  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _))
      .WillOnce([](const media::AudioBus& input, media::AudioBus& output) {
        input.CopyTo(&output);
      });

  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get()));

  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  // Volume adjustment should be dispatched directly to volume_callback_ by
  // AudioProcessorHandler, while deliver_callback_ receives audio from
  // VoiceIsolationHandler without volume.
  EXPECT_CALL(volume_callback_, Run(testing::Gt(0.0)))
      .WillOnce([&](double new_volume) { run_loop.Quit(); });
  EXPECT_CALL(deliver_callback_, Run(_, _, _));

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                /*volume=*/0.0, media::AudioGlitchInfo());
  run_loop.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       CallingSetVoiceIsolationWhileProcessingTogglesVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.voice_isolation = true;

  auto mock_component = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr = mock_component.get();

  mojo::Remote<media::mojom::AudioProcessorControls> remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), volume_callback_.Get(), error_callback_.Get(),
      remote.BindNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      CreateVoiceIsolationHandlerWithMock(
          std::move(mock_component), output_params_, deliver_callback_.Get()));

  handler->StartProcessing();
  EXPECT_TRUE(HasVoiceIsolationHandler(*handler));

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  // 1. Voice isolation is enabled by default.
  // We expect the mock component to be called when processing captured audio.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
    EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
    EXPECT_CALL(deliver_callback_, Run(_, _, _))
        .WillOnce([&](const media::AudioBus& processed_bus,
                      base::TimeTicks capture_time,
                      const media::AudioGlitchInfo& glitch_info) {
          run_loop.Quit();
        });
    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                  media::AudioGlitchInfo());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
    testing::Mock::VerifyAndClearExpectations(&deliver_callback_);
  }

  // 2. Disable voice isolation.
  remote->SetVoiceIsolation(false);
  remote.FlushForTesting();

  // On the first frame after disabling, the mock component processes audio
  // once more to crossfade to the microphone signal, and ClearBuffers() is
  // called once to purge lookahead frames.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
    EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(1);
    EXPECT_CALL(deliver_callback_, Run(_, _, _))
        .WillOnce([&](const media::AudioBus& processed_bus,
                      base::TimeTicks capture_time,
                      const media::AudioGlitchInfo& glitch_info) {
          run_loop.Quit();
        });
    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                  media::AudioGlitchInfo());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
    testing::Mock::VerifyAndClearExpectations(&deliver_callback_);
  }

  // 3. Re-enable voice isolation.
  remote->SetVoiceIsolation(true);
  remote.FlushForTesting();

  // With voice isolation re-enabled, the mock component should be called again.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
    EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
    EXPECT_CALL(deliver_callback_, Run(_, _, _))
        .WillOnce([&](const media::AudioBus& processed_bus,
                      base::TimeTicks capture_time,
                      const media::AudioGlitchInfo& glitch_info) {
          run_loop.Quit();
        });
    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                  media::AudioGlitchInfo());
    run_loop.Run();
  }

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       CallingSetVoiceIsolationWithoutHandlerReportsBadMessage) {
  media::AudioProcessingSettings settings;
  mojo::Remote<media::mojom::AudioProcessorControls> remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), volume_callback_.Get(), error_callback_.Get(),
      remote.BindNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation_handler=*/nullptr);

  std::string bad_message;
  mojo::SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { bad_message = error; }));

  // Disabling voice isolation when it is not available is a no-op and should
  // not report a bad message.
  remote->SetVoiceIsolation(false);
  remote.FlushForTesting();
  EXPECT_TRUE(bad_message.empty());

  // Enabling voice isolation when it is not available reports a bad message.
  remote->SetVoiceIsolation(true);
  remote.FlushForTesting();

  EXPECT_EQ(bad_message, "Voice isolation cannot be enabled.");
  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
}
#endif

TEST_F(AudioProcessorHandlerTest, NoVolumeAdjustmentOnSilence) {
  media::AudioProcessingSettings settings;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation_handler=*/nullptr);

  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  // An arbitrary, non-trivial volume level in the range (0.0, 1.0).
  double volume = 0.789;
  base::RunLoop run_loop;
  // The volume adjustment callback is only called if the AGC recommends a
  // volume adjustment. Since the input is silent, the AGC recommends no change,
  // so volume_callback_ should not be called.
  EXPECT_CALL(volume_callback_, Run(_)).Times(0);
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce(
          [&](const media::AudioBus& processed_bus,
              base::TimeTicks capture_time,
              const media::AudioGlitchInfo& glitch_info) { run_loop.Quit(); });

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), volume,
                                media::AudioGlitchInfo());
  run_loop.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest, VolumeAdjustmentRecommendedByAgc) {
  media::AudioProcessingSettings settings;
  settings.automatic_gain_control = true;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation_handler=*/nullptr);

  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  // At startup with zero volume, WebRTC AGC enforces a minimum input volume,
  // recommending an upward adjustment.
  base::RunLoop run_loop;
  EXPECT_CALL(volume_callback_, Run(testing::Gt(0.0)))
      .WillOnce([&](double new_volume) { run_loop.Quit(); });
  EXPECT_CALL(deliver_callback_, Run(_, _, _));

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                /*volume=*/0.0, media::AudioGlitchInfo());
  run_loop.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest, GlitchInfoAccumulation) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation_handler=*/nullptr);

  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  media::AudioGlitchInfo glitch_info1{.duration = base::Milliseconds(10),
                                      .count = 2};
  media::AudioGlitchInfo glitch_info2{.duration = base::Milliseconds(5),
                                      .count = 1};

  EXPECT_CALL(deliver_callback_, Run(_, _, glitch_info1)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info1);

  EXPECT_CALL(deliver_callback_, Run(_, _, glitch_info2)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info2);

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest, GlitchInfoAccumulationWithFifo) {
  media::AudioProcessingSettings settings;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation_handler=*/nullptr);

  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  media::AudioGlitchInfo glitch_info1{.duration = base::Milliseconds(10),
                                      .count = 2};
  media::AudioGlitchInfo glitch_info2{.duration = base::Milliseconds(5),
                                      .count = 1};

  // We push two frames into the FIFO. They will be processed sequentially by
  // the FIFO thread. The first frame has glitch_info1, the second has
  // glitch_info2. Wait, because we are pushing two frames before running the
  // loop, they might get processed in one or two callbacks depending on timing.
  // But since they are processed sequentially, let's check how the FIFO thread
  // processes them: For each frame popped from FIFO, it calls
  // ProcessCapturedAudioInternal, which adds the glitch to the accumulator,
  // then calls audio_processor_->ProcessCapturedAudio, which runs
  // OnAudioProcessorOutput, which gets the accumulated glitch info and resets
  // the accumulator. So they are processed as two separate output frames, each
  // with their own glitch! Wait! If they are two separate frames, then
  // `deliver_callback_` will be called TWICE! The first call gets glitch_info1,
  // and the second gets glitch_info2! Let's verify this behavior:
  base::RunLoop run_loop1;
  EXPECT_CALL(deliver_callback_, Run(_, _, glitch_info1)).WillOnce([&]() {
    run_loop1.Quit();
  });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info1);
  run_loop1.Run();

  base::RunLoop run_loop2;
  EXPECT_CALL(deliver_callback_, Run(_, _, glitch_info2)).WillOnce([&]() {
    run_loop2.Quit();
  });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info2);
  run_loop2.Run();

  handler->StopProcessing();
}

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
TEST_F(AudioProcessorHandlerTest,
       GlitchInfoAccumulationWithVoiceIsolationHandler) {
  media::AudioProcessingSettings settings;
  settings.voice_isolation = true;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr =
      mock_voice_isolation.get();

  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(2);

  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get()));

  handler->StartProcessing();
  EXPECT_TRUE(HasVoiceIsolationHandler(*handler));

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  media::AudioGlitchInfo glitch_info1{.duration = base::Milliseconds(10),
                                      .count = 2};
  media::AudioGlitchInfo glitch_info2{.duration = base::Milliseconds(5),
                                      .count = 1};
  base::RunLoop run_loop1;
  EXPECT_CALL(deliver_callback_, Run(_, _, glitch_info1)).WillOnce([&]() {
    run_loop1.Quit();
  });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info1);
  run_loop1.Run();

  base::RunLoop run_loop2;
  EXPECT_CALL(deliver_callback_, Run(_, _, glitch_info2)).WillOnce([&]() {
    run_loop2.Quit();
  });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info2);
  run_loop2.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerMaybeCreateRegistersDebugRecorder) {
  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([&]() { return base::MakeRefCounted<FakeMlModelHandle>(); });
  media::MockAudioDebugRecordingManager mock_debug_recording_manager;

  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get(),
      /*log_callback=*/base::DoNothing(), &mock_debug_recording_manager);
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->HasDebugRecorderForTesting());
}

TEST_F(AudioProcessorHandlerTest, VoiceIsolationDebugRecordingCapturesData) {
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  auto mock_recorder =
      std::make_unique<testing::StrictMock<MockAudioDebugRecorder>>();
  EXPECT_CALL(*mock_recorder, OnData(_)).Times(1);

  auto handler = CreateVoiceIsolationHandlerWithMock(
      std::move(mock_voice_isolation), output_params_, deliver_callback_.Get(),
      std::move(mock_recorder));

  auto input_bus = media::AudioBus::Create(output_params_);
  input_bus->Zero();
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                media::AudioGlitchInfo());
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationDebugRecordingCapturesDataWhenBypassed) {
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  auto mock_recorder =
      std::make_unique<testing::StrictMock<MockAudioDebugRecorder>>();
  EXPECT_CALL(*mock_recorder, OnData(_)).Times(1);

  auto handler = CreateVoiceIsolationHandlerWithMock(
      std::move(mock_voice_isolation), output_params_, deliver_callback_.Get(),
      std::move(mock_recorder));

  handler->SetVoiceIsolation(false);
  EXPECT_TRUE(handler->IsVoiceIsolationBypassedForTesting());

  auto input_bus = media::AudioBus::Create(output_params_);
  input_bus->Zero();
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                media::AudioGlitchInfo());
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerMaybeCreateReturnsNullIfModelManagerReturnsNull) {
  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([]() { return nullptr; });
  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get());
  EXPECT_FALSE(handler);
}

TEST_F(AudioProcessorHandlerTest, VoiceIsolationHandlerMaybeCreateSuccess) {
  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([&]() { return base::MakeRefCounted<FakeMlModelHandle>(); });
  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get());
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->IsVoiceIsolationBypassedForTesting());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return handler->IsInitializedForTesting(); }));
  EXPECT_FALSE(handler->IsVoiceIsolationBypassedForTesting());
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerPassThroughDuringAsyncInitialization) {
  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([&]() { return base::MakeRefCounted<FakeMlModelHandle>(); });
  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get());
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->IsVoiceIsolationBypassedForTesting());

  auto input_bus = media::AudioBus::Create(output_params_);
  input_bus->Zero();

  EXPECT_CALL(deliver_callback_, Run(testing::Ref(*input_bus), _, _));
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return handler->IsInitializedForTesting(); }));
  EXPECT_FALSE(handler->IsVoiceIsolationBypassedForTesting());

  bool delivered_same_instance = true;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& bus, base::TimeTicks,
                    const media::AudioGlitchInfo&) {
        delivered_same_instance = (&bus == input_bus.get());
      });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  EXPECT_FALSE(delivered_same_instance);
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerDisableWhileInitializing) {
  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([&]() { return base::MakeRefCounted<FakeMlModelHandle>(); });
  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get());
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->IsVoiceIsolationBypassedForTesting());

  // Redundant enable call is a no-op.
  handler->SetVoiceIsolation(true);
  EXPECT_TRUE(handler->IsVoiceIsolationBypassedForTesting());

  // Disable voice isolation while initialization is in flight.
  handler->SetVoiceIsolation(false);
  // Redundant disable call is a no-op.
  handler->SetVoiceIsolation(false);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return handler->IsInitializedForTesting(); }));
  EXPECT_TRUE(handler->IsVoiceIsolationBypassedForTesting());
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerDestroyWhileInitializing) {
  base::RunLoop run_loop;
  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([&]() {
        return base::MakeRefCounted<FakeMlModelHandle>(run_loop.QuitClosure());
      });
  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get());
  ASSERT_TRUE(handler);

  handler.reset();
  run_loop.Run();
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerAsyncStartupSuccessMetrics) {
  base::HistogramTester histogram_tester;
  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([&]() { return base::MakeRefCounted<FakeMlModelHandle>(); });
  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get());
  ASSERT_TRUE(handler);
  EXPECT_FALSE(handler->IsInitializedForTesting());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return handler->IsInitializedForTesting(); }));

  // Metrics are logged when startup_metrics_logger_ is destroyed in
  // OnComponentCreated().
  histogram_tester.ExpectUniqueSample(
      "Media.Audio.Capture.VoiceIsolation.StartupResult",
      VoiceIsolationStartupResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.Capture.VoiceIsolation.StartupDuration.Success", 1);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.Capture.VoiceIsolation.StartupDuration.Failure", 0);
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerAsyncStartupAbortedMetrics) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([&]() {
        return base::MakeRefCounted<FakeMlModelHandle>(run_loop.QuitClosure());
      });
  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get());
  ASSERT_TRUE(handler);
  EXPECT_FALSE(handler->IsInitializedForTesting());

  handler.reset();
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "Media.Audio.Capture.VoiceIsolation.StartupResult",
      VoiceIsolationStartupResult::kAborted, 1);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.Capture.VoiceIsolation.StartupDuration.Success", 0);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.Capture.VoiceIsolation.StartupDuration.Failure", 0);
}

TEST_F(AudioProcessorHandlerTest, NoDedicatedFifoByDefault) {
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  auto handler = CreateVoiceIsolationHandlerWithMock(
      std::move(mock_voice_isolation), output_params_, deliver_callback_.Get());
  EXPECT_FALSE(handler->HasProcessingThread());
  EXPECT_EQ(handler->GetFifoSizeForTesting(), 0);
}

TEST_F(AudioProcessorHandlerTest,
       DedicatedFifoCreatedWithDefaultSizeWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  auto handler = CreateVoiceIsolationHandlerWithMock(
      std::move(mock_voice_isolation), output_params_, deliver_callback_.Get());
  EXPECT_TRUE(handler->HasProcessingThread());
  EXPECT_EQ(handler->GetFifoSizeForTesting(), 10);
}

TEST_F(AudioProcessorHandlerTest,
       DedicatedFifoSizeConfiguredByFeatureParamAndClamped) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        media::kWebRtcVoiceIsolationProcessingFifo, {{"fifo_size", "5"}});
    auto handler = CreateVoiceIsolationHandlerWithMock(
        std::make_unique<media::MockVoiceIsolation>(), output_params_,
        deliver_callback_.Get());
    EXPECT_EQ(handler->GetFifoSizeForTesting(), 5);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        media::kWebRtcVoiceIsolationProcessingFifo, {{"fifo_size", "0"}});
    auto handler = CreateVoiceIsolationHandlerWithMock(
        std::make_unique<media::MockVoiceIsolation>(), output_params_,
        deliver_callback_.Get());
    EXPECT_EQ(handler->GetFifoSizeForTesting(), 1);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        media::kWebRtcVoiceIsolationProcessingFifo, {{"fifo_size", "500"}});
    auto handler = CreateVoiceIsolationHandlerWithMock(
        std::make_unique<media::MockVoiceIsolation>(), output_params_,
        deliver_callback_.Get());
    EXPECT_EQ(handler->GetFifoSizeForTesting(), 100);
  }
}

TEST_F(AudioProcessorHandlerTest, AudioProcessedAndDeliveredViaDedicatedFifo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* mock_ptr = mock_voice_isolation.get();

  EXPECT_CALL(*mock_ptr, ProcessAudio(_, _))
      .WillOnce([](const media::AudioBus& input, media::AudioBus& output) {
        input.CopyTo(&output);
      });

  base::RunLoop run_loop;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(processed_bus.channels(), output_params_.channels());
        EXPECT_EQ(processed_bus.frames(), output_params_.frames_per_buffer());
        EXPECT_EQ(glitch_info.duration, base::Milliseconds(10));
        EXPECT_EQ(glitch_info.count, 1u);
        run_loop.Quit();
      });

  auto handler = CreateVoiceIsolationHandlerWithMock(
      std::move(mock_voice_isolation), output_params_, deliver_callback_.Get());
  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(output_params_);
  input_bus->Zero();
  media::AudioGlitchInfo input_glitch_info{.duration = base::Milliseconds(10),
                                           .count = 1};
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                input_glitch_info);
  run_loop.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest, StopProcessingResetsDedicatedFifo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  auto handler = CreateVoiceIsolationHandlerWithMock(
      std::make_unique<media::MockVoiceIsolation>(), output_params_,
      deliver_callback_.Get());
  EXPECT_TRUE(handler->HasProcessingThread());
  EXPECT_EQ(handler->GetFifoSizeForTesting(), 10);

  handler->StartProcessing();
  handler->StopProcessing();

  EXPECT_FALSE(handler->HasProcessingThread());
  EXPECT_EQ(handler->GetFifoSizeForTesting(), 0);
}

TEST_F(AudioProcessorHandlerTest,
       SingleFifoWhenOnlyDedicatedVoiceIsolationActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;
  settings.voice_isolation = true;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();

  auto audio_processor_handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get()));

  EXPECT_FALSE(audio_processor_handler->needs_playout_reference());
  EXPECT_TRUE(HasVoiceIsolationHandler(*audio_processor_handler));
  // VoiceIsolationHandler has its own dedicated processing thread, so
  // AudioProcessorHandler does NOT need to create a ProcessingAudioFifo when
  // echo cancellation is disabled. Exactly one FIFO exists across the pipeline.
  EXPECT_FALSE(HasProcessingFifo(*audio_processor_handler));
  EXPECT_TRUE(VoiceIsolationHasProcessingThread(*audio_processor_handler));
  EXPECT_EQ(GetVoiceIsolationFifoSize(*audio_processor_handler), 10);

  audio_processor_handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(processed_bus.channels(), output_params_.channels());
        EXPECT_EQ(processed_bus.frames(), output_params_.frames_per_buffer());
        run_loop.Quit();
      });

  audio_processor_handler->ProcessCapturedAudio(
      *input_bus, base::TimeTicks::Now(), 1.0, media::AudioGlitchInfo());
  run_loop.Run();

  audio_processor_handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       TwoStageFifoWhenAecAndDedicatedVoiceIsolationBothActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  media::AudioProcessingSettings settings;
  settings.echo_cancellation = true;
  settings.voice_isolation = true;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();

  auto audio_processor_handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get()));

  EXPECT_TRUE(audio_processor_handler->needs_playout_reference());
  EXPECT_TRUE(HasVoiceIsolationHandler(*audio_processor_handler));
  // AudioProcessorHandler creates its FIFO for echo cancellation, and
  // VoiceIsolationHandler uses its own dedicated FIFO (decoupled two-stage
  // pipeline).
  EXPECT_TRUE(HasProcessingFifo(*audio_processor_handler));
  EXPECT_TRUE(VoiceIsolationHasProcessingThread(*audio_processor_handler));
  EXPECT_EQ(GetVoiceIsolationFifoSize(*audio_processor_handler), 10);

  audio_processor_handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(processed_bus.channels(), output_params_.channels());
        EXPECT_EQ(processed_bus.frames(), output_params_.frames_per_buffer());
        run_loop.Quit();
      });

  audio_processor_handler->ProcessCapturedAudio(
      *input_bus, base::TimeTicks::Now(), 1.0, media::AudioGlitchInfo());
  run_loop.Run();

  audio_processor_handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       GlitchInfoAccumulatesWithDedicatedVoiceIsolationFifo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* mock_ptr = mock_voice_isolation.get();
  EXPECT_CALL(*mock_ptr, ProcessAudio(_, _))
      .WillRepeatedly([](const media::AudioBus& input,
                         media::AudioBus& output) { input.CopyTo(&output); });

  auto handler = CreateVoiceIsolationHandlerWithMock(
      std::move(mock_voice_isolation), output_params_, deliver_callback_.Get());
  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(output_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(glitch_info.duration, base::Milliseconds(15));
        EXPECT_EQ(glitch_info.count, 2u);
        run_loop.Quit();
      });

  media::AudioGlitchInfo glitches{
      .duration = base::Milliseconds(15),
      .count = 2,
  };
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), glitches);
  run_loop.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       FifoCreatedWhenVoiceIsolationActiveWithoutPlayoutReference) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;
  settings.voice_isolation = true;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr =
      mock_voice_isolation.get();

  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _))
      .WillOnce([](const media::AudioBus& input, media::AudioBus& output) {
        input.CopyTo(&output);
      });

  auto audio_processor_handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get()));

  EXPECT_FALSE(audio_processor_handler->needs_playout_reference());
  EXPECT_TRUE(HasVoiceIsolationHandler(*audio_processor_handler));
  // When kWebRtcVoiceIsolationProcessingFifo is disabled (default),
  // VoiceIsolationHandler does not have its own processing thread, so
  // AudioProcessorHandler must create a ProcessingAudioFifo to offload voice
  // isolation from the capture device thread even though echo cancellation is
  // disabled.
  EXPECT_TRUE(HasProcessingFifo(*audio_processor_handler));
  EXPECT_FALSE(VoiceIsolationHasProcessingThread(*audio_processor_handler));
  EXPECT_EQ(GetVoiceIsolationFifoSize(*audio_processor_handler), 0);

  audio_processor_handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(processed_bus.channels(), output_params_.channels());
        EXPECT_EQ(processed_bus.frames(), output_params_.frames_per_buffer());
        run_loop.Quit();
      });

  audio_processor_handler->ProcessCapturedAudio(
      *input_bus, base::TimeTicks::Now(), 1.0, media::AudioGlitchInfo());
  run_loop.Run();

  audio_processor_handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       AudioPassesThroughDuringAsyncInitializationWithDedicatedFifo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  MockMlModelManager model_manager;
  EXPECT_CALL(model_manager,
              GetModel(mojom::MlModelType::kVoiceIsolationDenoiser))
      .WillOnce([&]() { return base::MakeRefCounted<FakeMlModelHandle>(); });
  auto handler = VoiceIsolationHandler::MaybeCreate(
      model_manager, output_params_, deliver_callback_.Get());
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->HasProcessingThread());
  EXPECT_TRUE(handler->IsVoiceIsolationBypassedForTesting());

  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(output_params_);
  input_bus->Zero();

  base::RunLoop run_loop1;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& bus, base::TimeTicks,
                    const media::AudioGlitchInfo&) {
        EXPECT_EQ(bus.channels(), output_params_.channels());
        EXPECT_EQ(bus.frames(), output_params_.frames_per_buffer());
        run_loop1.Quit();
      });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  run_loop1.Run();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return handler->IsInitializedForTesting(); }));
  EXPECT_FALSE(handler->IsVoiceIsolationBypassedForTesting());

  base::RunLoop run_loop2;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillOnce([&](const media::AudioBus& bus, base::TimeTicks,
                    const media::AudioGlitchInfo&) {
        EXPECT_EQ(bus.channels(), output_params_.channels());
        EXPECT_EQ(bus.frames(), output_params_.frames_per_buffer());
        run_loop2.Quit();
      });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  run_loop2.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       AudioProcessedOnlyWhenVoiceIsolationEnabledWithDedicatedFifo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* mock_ptr = mock_voice_isolation.get();

  auto handler = CreateVoiceIsolationHandlerWithMock(
      std::move(mock_voice_isolation), output_params_, deliver_callback_.Get());
  EXPECT_TRUE(handler->HasProcessingThread());
  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(output_params_);
  input_bus->Zero();

  // 1. Voice isolation enabled (default): ProcessAudio is called.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_ptr, ProcessAudio(_, _))
        .WillOnce([](const media::AudioBus& input, media::AudioBus& output) {
          input.CopyTo(&output);
        });
    EXPECT_CALL(deliver_callback_, Run(_, _, _)).WillOnce([&]() {
      run_loop.Quit();
    });
    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
    run_loop.Run();
  }

  // 2. Disable voice isolation: the first frame runs ProcessAudio once more to
  // crossfade to the microphone signal.
  handler->SetVoiceIsolation(false);
  EXPECT_TRUE(handler->IsVoiceIsolationBypassedForTesting());
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_ptr, ProcessAudio(_, _)).Times(1);
    EXPECT_CALL(*mock_ptr, ClearBuffers()).Times(1);
    EXPECT_CALL(deliver_callback_, Run(_, _, _)).WillOnce([&]() {
      run_loop.Quit();
    });
    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
    run_loop.Run();
  }

  // 3. Re-enable voice isolation: ProcessAudio is called again.
  handler->SetVoiceIsolation(true);
  EXPECT_FALSE(handler->IsVoiceIsolationBypassedForTesting());
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_ptr, ProcessAudio(_, _))
        .WillOnce([](const media::AudioBus& input, media::AudioBus& output) {
          input.CopyTo(&output);
        });
    EXPECT_CALL(deliver_callback_, Run(_, _, _)).WillOnce([&]() {
      run_loop.Quit();
    });
    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
    run_loop.Run();
  }

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       NoFifoWithoutPlayoutReferenceOrVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto audio_processor_handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), volume_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation_handler=*/nullptr);

  EXPECT_FALSE(audio_processor_handler->needs_playout_reference());
  EXPECT_FALSE(HasVoiceIsolationHandler(*audio_processor_handler));
  EXPECT_FALSE(HasProcessingFifo(*audio_processor_handler));
  EXPECT_FALSE(VoiceIsolationHasProcessingThread(*audio_processor_handler));
  EXPECT_EQ(GetVoiceIsolationFifoSize(*audio_processor_handler), 0);
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerClearBuffersTriggeredOnTransitionToBypassed) {
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr =
      mock_voice_isolation.get();

  std::unique_ptr<VoiceIsolationHandler> handler =
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get());
  ASSERT_TRUE(handler);

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  // 1. Initial active processing: voice isolation is enabled by default in the
  // test constructor. ProcessAudio is called, ClearBuffers is NOT called.
  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
  EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
  EXPECT_CALL(deliver_callback_, Run(_, _, _)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                media::AudioGlitchInfo());
  testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
  testing::Mock::VerifyAndClearExpectations(&deliver_callback_);

  // 2. Disable voice isolation.
  handler->SetVoiceIsolation(false);

  // 3. First captured frame after disabling: ON -> OFF transition runs voice
  // isolation once to crossfade, then triggers ClearBuffers() exactly once.
  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
  EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(1);
  EXPECT_CALL(deliver_callback_,
              Run(testing::Not(testing::Ref(*input_bus)), _, _))
      .Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                media::AudioGlitchInfo());
  testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
  testing::Mock::VerifyAndClearExpectations(&deliver_callback_);

  // 4. Subsequent frame while bypassed: ClearBuffers() must NOT be called again
  // (idempotent, avoids redundant clearing work).
  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(0);
  EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
  EXPECT_CALL(deliver_callback_, Run(testing::Ref(*input_bus), _, _)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                media::AudioGlitchInfo());
  testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
  testing::Mock::VerifyAndClearExpectations(&deliver_callback_);

  // 5. Re-enable voice isolation: processing resumes.
  handler->SetVoiceIsolation(true);
  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
  EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
  EXPECT_CALL(deliver_callback_, Run(_, _, _)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                media::AudioGlitchInfo());
  testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
  testing::Mock::VerifyAndClearExpectations(&deliver_callback_);

  // 6. Disable again: crossfades and triggers ClearBuffers() once more.
  handler->SetVoiceIsolation(false);
  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
  EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(1);
  EXPECT_CALL(deliver_callback_,
              Run(testing::Not(testing::Ref(*input_bus)), _, _))
      .Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                media::AudioGlitchInfo());
  testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
  testing::Mock::VerifyAndClearExpectations(&deliver_callback_);
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerNoClearBuffersWhenBypassedFromStart) {
  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr =
      mock_voice_isolation.get();

  std::unique_ptr<VoiceIsolationHandler> handler =
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get());
  ASSERT_TRUE(handler);

  // Disable before any audio frames are captured.
  handler->SetVoiceIsolation(false);

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  // Since it was never actively processing (was_previously_bypassed_ was true),
  // ClearBuffers() should not be called.
  EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(0);
  EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
  EXPECT_CALL(deliver_callback_, Run(testing::Ref(*input_bus), _, _)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                media::AudioGlitchInfo());
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerClearBuffersWithProcessingFifo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebRtcVoiceIsolationProcessingFifo);

  auto mock_voice_isolation = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr =
      mock_voice_isolation.get();

  std::unique_ptr<VoiceIsolationHandler> handler =
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get());
  ASSERT_TRUE(handler);
  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  // 1. Process one frame with FIFO enabled.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
    EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
    EXPECT_CALL(deliver_callback_, Run(_, _, _))
        .WillOnce([&](const media::AudioBus&, base::TimeTicks,
                      const media::AudioGlitchInfo&) { run_loop.Quit(); });

    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                  media::AudioGlitchInfo());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
    testing::Mock::VerifyAndClearExpectations(&deliver_callback_);
  }

  // 2. Disable voice isolation.
  handler->SetVoiceIsolation(false);

  // 3. When the next frame is processed on the FIFO thread, ClearBuffers() must
  // be called once, after a final crossfade frame, to purge lookahead frames.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
    EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(1);
    EXPECT_CALL(deliver_callback_, Run(_, _, _))
        .WillOnce([&](const media::AudioBus&, base::TimeTicks,
                      const media::AudioGlitchInfo&) { run_loop.Quit(); });

    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                  media::AudioGlitchInfo());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
    testing::Mock::VerifyAndClearExpectations(&deliver_callback_);
  }

  // 4. Subsequent frame while bypassed: ClearBuffers() must NOT be called again
  // (idempotent, avoids redundant clearing work on the FIFO thread).
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(0);
    EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
    EXPECT_CALL(deliver_callback_, Run(_, _, _))
        .WillOnce([&](const media::AudioBus&, base::TimeTicks,
                      const media::AudioGlitchInfo&) { run_loop.Quit(); });

    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                  media::AudioGlitchInfo());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
    testing::Mock::VerifyAndClearExpectations(&deliver_callback_);
  }

  // 5. Re-enable voice isolation: processing resumes.
  handler->SetVoiceIsolation(/*enabled=*/true);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(1);
    EXPECT_CALL(*voice_isolation_mock_ptr, ClearBuffers()).Times(0);
    EXPECT_CALL(deliver_callback_, Run(_, _, _))
        .WillOnce([&](const media::AudioBus&, base::TimeTicks,
                      const media::AudioGlitchInfo&) { run_loop.Quit(); });

    handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                  media::AudioGlitchInfo());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(voice_isolation_mock_ptr);
    testing::Mock::VerifyAndClearExpectations(&deliver_callback_);
  }

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerCrossfadesMicToDenoisedOnEnable) {
  // The fade logic is thread-agnostic; pin the synchronous (no FIFO) path.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      media::kWebRtcVoiceIsolationProcessingFifo);

  std::unique_ptr<media::MockVoiceIsolation> mock_voice_isolation =
      std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* mock_ptr = mock_voice_isolation.get();

  std::unique_ptr<VoiceIsolationHandler> handler =
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get());
  ASSERT_TRUE(handler);

  // Must be set after CreateVoiceIsolationHandlerWithMock() to override its
  // pass-through default action. Records the voice isolation input.
  std::unique_ptr<media::AudioBus> voice_isolation_input =
      media::AudioBus::Create(output_params_);
  ON_CALL(*mock_ptr, ProcessAudio(_, _))
      .WillByDefault(
          [&](const media::AudioBus& input, media::AudioBus& output) {
            input.CopyTo(voice_isolation_input.get());
            FillBus(output, kDenoisedLevel);
          });

  std::unique_ptr<media::AudioBus> input_bus =
      media::AudioBus::Create(input_params_);
  FillBus(*input_bus, kMicLevel);
  std::vector<float> delivered(kFramesPerBuffer);
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillRepeatedly([&](const media::AudioBus& bus, base::TimeTicks,
                          const media::AudioGlitchInfo&) {
        base::span(delivered).copy_from(bus.channel(0));
      });

  // First active frame (OFF -> ON edge).
  EXPECT_CALL(*mock_ptr, ProcessAudio(_, _)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  testing::Mock::VerifyAndClearExpectations(mock_ptr);

  // Voice isolation is fed the unmodified microphone signal: its cleared
  // internal state already makes its output ramp up from silence.
  for (float sample : voice_isolation_input->channel(0)) {
    EXPECT_FLOAT_EQ(sample, kMicLevel);
  }

  // The delivered signal crossfades from the microphone signal to the voice
  // isolation output.
  for (int i = 0; i < kFramesPerBuffer; ++i) {
    const float fade_in = ExpectedFadeIn(i, kFramesPerBuffer);
    EXPECT_NEAR(delivered[i],
                (1.0f - fade_in) * kMicLevel + fade_in * kDenoisedLevel,
                kFadeTolerance)
        << "sample " << i;
  }

  // Oracle-independent anchors: continuous with the bypassed microphone
  // signal, halfway at mid-frame, strictly monotonic, and ending exactly on
  // the voice isolation output.
  EXPECT_NEAR(delivered.front(), kMicLevel, kFadeEdgeTolerance);
  EXPECT_NEAR(delivered[kMidFrameIndex], 0.5f * (kMicLevel + kDenoisedLevel),
              kFadeTolerance);
  for (int i = 1; i < kFramesPerBuffer; ++i) {
    EXPECT_LT(delivered[i], delivered[i - 1]) << "sample " << i;
  }
  EXPECT_NEAR(delivered.back(), kDenoisedLevel, kFadeTolerance);

  // Steady state: the unmodified microphone signal is fed to voice isolation
  // and its output is delivered untouched.
  EXPECT_CALL(*mock_ptr, ProcessAudio(_, _)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  for (float sample : voice_isolation_input->channel(0)) {
    EXPECT_FLOAT_EQ(sample, kMicLevel);
  }
  for (float sample : delivered) {
    EXPECT_FLOAT_EQ(sample, kDenoisedLevel);
  }
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerCrossfadesToMicAndClearsBuffersOnDisable) {
  // The fade logic is thread-agnostic; pin the synchronous (no FIFO) path.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      media::kWebRtcVoiceIsolationProcessingFifo);

  std::unique_ptr<media::MockVoiceIsolation> mock_voice_isolation =
      std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* mock_ptr = mock_voice_isolation.get();

  std::unique_ptr<VoiceIsolationHandler> handler =
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get());
  ASSERT_TRUE(handler);

  // Must be set after CreateVoiceIsolationHandlerWithMock() to override its
  // pass-through default action.
  ON_CALL(*mock_ptr, ProcessAudio(_, _))
      .WillByDefault([](const media::AudioBus&, media::AudioBus& output) {
        FillBus(output, kDenoisedLevel);
      });

  std::unique_ptr<media::AudioBus> input_bus =
      media::AudioBus::Create(input_params_);
  FillBus(*input_bus, kMicLevel);
  std::vector<float> delivered(kFramesPerBuffer);
  const media::AudioBus* delivered_bus = nullptr;
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillRepeatedly([&](const media::AudioBus& bus, base::TimeTicks,
                          const media::AudioGlitchInfo&) {
        delivered_bus = &bus;
        base::span(delivered).copy_from(bus.channel(0));
      });

  // Reach steady-state active processing (first frame is the OFF -> ON fade).
  EXPECT_CALL(*mock_ptr, ProcessAudio(_, _)).Times(2);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  testing::Mock::VerifyAndClearExpectations(mock_ptr);

  // ON -> OFF edge: one final ProcessAudio(), crossfade, then ClearBuffers().
  handler->SetVoiceIsolation(false);
  {
    testing::InSequence sequence;
    EXPECT_CALL(*mock_ptr, ProcessAudio(_, _)).Times(1);
    EXPECT_CALL(*mock_ptr, ClearBuffers()).Times(1);
  }
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  EXPECT_NE(delivered_bus, input_bus.get());
  for (int i = 0; i < kFramesPerBuffer; ++i) {
    const float fade_in = ExpectedFadeIn(i, kFramesPerBuffer);
    EXPECT_NEAR(delivered[i],
                (1.0f - fade_in) * kDenoisedLevel + fade_in * kMicLevel,
                kFadeTolerance)
        << "sample " << i;
  }

  // Oracle-independent anchors: continuous with the previous voice isolation
  // output, halfway at mid-frame, strictly monotonic, and ending exactly on
  // the microphone signal.
  EXPECT_NEAR(delivered.front(), kDenoisedLevel, kFadeEdgeTolerance);
  EXPECT_NEAR(delivered[kMidFrameIndex], 0.5f * (kMicLevel + kDenoisedLevel),
              kFadeTolerance);
  for (int i = 1; i < kFramesPerBuffer; ++i) {
    EXPECT_GT(delivered[i], delivered[i - 1]) << "sample " << i;
  }
  EXPECT_NEAR(delivered.back(), kMicLevel, kFadeTolerance);
  testing::Mock::VerifyAndClearExpectations(mock_ptr);

  // Steady bypass: zero-copy pass-through, no processing, no clearing.
  EXPECT_CALL(*mock_ptr, ProcessAudio(_, _)).Times(0);
  EXPECT_CALL(*mock_ptr, ClearBuffers()).Times(0);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  EXPECT_EQ(delivered_bus, input_bus.get());
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerTransitionsAreContinuousOnRapidToggle) {
  // The fade logic is thread-agnostic; pin the synchronous (no FIFO) path.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      media::kWebRtcVoiceIsolationProcessingFifo);

  std::unique_ptr<media::MockVoiceIsolation> mock_voice_isolation =
      std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* mock_ptr = mock_voice_isolation.get();

  std::unique_ptr<VoiceIsolationHandler> handler =
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          output_params_,
                                          deliver_callback_.Get());
  ASSERT_TRUE(handler);

  // Must be set after CreateVoiceIsolationHandlerWithMock() to override its
  // pass-through default action.
  ON_CALL(*mock_ptr, ProcessAudio(_, _))
      .WillByDefault([](const media::AudioBus&, media::AudioBus& output) {
        FillBus(output, kDenoisedLevel);
      });

  std::unique_ptr<media::AudioBus> input_bus =
      media::AudioBus::Create(input_params_);
  FillBus(*input_bus, kMicLevel);
  std::vector<float> delivered(kFramesPerBuffer);
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillRepeatedly([&](const media::AudioBus& bus, base::TimeTicks,
                          const media::AudioGlitchInfo&) {
        base::span(delivered).copy_from(bus.channel(0));
      });

  // Reach steady-state active processing.
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});

  // Frame N: ON -> OFF. Frame N + 1: OFF -> ON. Both frames run voice isolation
  // once, and buffers are cleared exactly once.
  EXPECT_CALL(*mock_ptr, ProcessAudio(_, _)).Times(2);
  EXPECT_CALL(*mock_ptr, ClearBuffers()).Times(1);

  handler->SetVoiceIsolation(false);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  const float last_sample_of_disable_frame = delivered.back();

  handler->SetVoiceIsolation(true);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  const float first_sample_of_enable_frame = delivered.front();

  // The ON -> OFF frame ends on the microphone signal and the OFF -> ON frame
  // starts from it, so there is no step across the frame boundary.
  EXPECT_NEAR(last_sample_of_disable_frame, kMicLevel, kFadeTolerance);
  EXPECT_NEAR(first_sample_of_enable_frame, kMicLevel, kFadeEdgeTolerance);
  EXPECT_NEAR(delivered.back(), kDenoisedLevel, kFadeTolerance);
}

TEST_F(AudioProcessorHandlerTest,
       VoiceIsolationHandlerTransitionsFadeEveryChannel) {
  // The fade logic is thread-agnostic; pin the synchronous (no FIFO) path.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      media::kWebRtcVoiceIsolationProcessingFifo);

  const media::AudioParameters stereo_params(
      media::AudioParameters::Format::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::Stereo(), kSampleRate, kFramesPerBuffer);
  // Distinct microphone level per channel to detect channel mix-ups.
  constexpr std::array<float, 2> kMicLevels = {kMicLevel, 0.5f * kMicLevel};

  std::unique_ptr<media::MockVoiceIsolation> mock_voice_isolation =
      std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* mock_ptr = mock_voice_isolation.get();

  std::unique_ptr<VoiceIsolationHandler> handler =
      CreateVoiceIsolationHandlerWithMock(std::move(mock_voice_isolation),
                                          stereo_params,
                                          deliver_callback_.Get());
  ASSERT_TRUE(handler);

  // Must be set after CreateVoiceIsolationHandlerWithMock() to override its
  // pass-through default action. Records the voice isolation input.
  std::unique_ptr<media::AudioBus> voice_isolation_input =
      media::AudioBus::Create(stereo_params);
  ON_CALL(*mock_ptr, ProcessAudio(_, _))
      .WillByDefault(
          [&](const media::AudioBus& input, media::AudioBus& output) {
            input.CopyTo(voice_isolation_input.get());
            FillBus(output, kDenoisedLevel);
          });

  std::unique_ptr<media::AudioBus> input_bus =
      media::AudioBus::Create(stereo_params);
  for (int ch = 0; ch < input_bus->channels(); ++ch) {
    std::ranges::fill(input_bus->channel(ch), kMicLevels[ch]);
  }
  std::unique_ptr<media::AudioBus> delivered =
      media::AudioBus::Create(stereo_params);
  EXPECT_CALL(deliver_callback_, Run(_, _, _))
      .WillRepeatedly(
          [&](const media::AudioBus& bus, base::TimeTicks,
              const media::AudioGlitchInfo&) { bus.CopyTo(delivered.get()); });

  // OFF -> ON edge: every channel is fed unmodified to voice isolation and
  // crossfaded from the microphone signal at the output.
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  for (int ch = 0; ch < stereo_params.channels(); ++ch) {
    for (int i = 0; i < kFramesPerBuffer; ++i) {
      const float fade_in = ExpectedFadeIn(i, kFramesPerBuffer);
      EXPECT_FLOAT_EQ(voice_isolation_input->channel(ch)[i], kMicLevels[ch])
          << "channel " << ch << ", sample " << i;
      EXPECT_NEAR(delivered->channel(ch)[i],
                  (1.0f - fade_in) * kMicLevels[ch] + fade_in * kDenoisedLevel,
                  kFadeTolerance)
          << "channel " << ch << ", sample " << i;
    }
  }

  // ON -> OFF edge: every channel is crossfaded back to its microphone signal.
  handler->SetVoiceIsolation(false);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), {});
  for (int ch = 0; ch < stereo_params.channels(); ++ch) {
    for (int i = 0; i < kFramesPerBuffer; ++i) {
      const float fade_in = ExpectedFadeIn(i, kFramesPerBuffer);
      EXPECT_NEAR(delivered->channel(ch)[i],
                  (1.0f - fade_in) * kDenoisedLevel + fade_in * kMicLevels[ch],
                  kFadeTolerance)
          << "channel " << ch << ", sample " << i;
    }
  }
}
#endif

}  // namespace

}  // namespace audio
