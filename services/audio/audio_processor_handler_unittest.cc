// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_processor_handler.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/base_paths.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/ml_model_handle.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
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
  base::MockCallback<AudioProcessorHandler::ReferenceStreamErrorCallback>
      error_callback_;

  bool HasVoiceIsolationHandler(const AudioProcessorHandler& handler) {
    return handler.voice_isolation_handler_ != nullptr;
  }
};

namespace {

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

class FakeMlModelHandle : public media::MlModelHandle {
 public:
  FakeMlModelHandle() {
    base::FilePath source_root;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));

    source_root = source_root.AppendASCII("media")
                      .AppendASCII("webrtc")
                      .AppendASCII("voice_isolation")
                      .AppendASCII("test_model_1_2_160_2.tflite");

    model_ = tflite::FlatBufferModel::BuildFromFile(
        source_root.AsUTF8Unsafe().c_str());
  }

  const tflite::FlatBufferModel& Get() override { return *model_; }

 private:
  ~FakeMlModelHandle() override = default;
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

std::unique_ptr<VoiceIsolationHandler> GetVoiceIsolationHandler(
    const media::AudioParameters& output_params,
    VoiceIsolationHandler::DeliverProcessedAudioCallback callback) {
  MockMlModelManager model_manager;

  return VoiceIsolationHandler::MaybeCreate(
      /*ml_model_manager=*/model_manager, output_params, callback);
}
#endif

TEST_F(AudioProcessorHandlerTest, SynchronousProcessingWithoutVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation=*/nullptr);

  handler->StartProcessing();
  EXPECT_FALSE(HasVoiceIsolationHandler(*handler));

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  // Without echo cancellation, processing happens synchronously on the capture
  // thread.
  EXPECT_CALL(deliver_callback_, Run(_, _, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time, std::optional<double> volume,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(processed_bus.channels(), output_params_.channels());
        EXPECT_EQ(processed_bus.frames(), output_params_.frames_per_buffer());
      });

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                media::AudioGlitchInfo());

  handler->StopProcessing();
}

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
TEST_F(AudioProcessorHandlerTest, SynchronousProcessingWithVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;
  settings.voice_isolation = true;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      GetVoiceIsolationHandler(output_params_, deliver_callback_.Get()));

  handler->StartProcessing();
  EXPECT_EQ(HasVoiceIsolationHandler(*handler), settings.voice_isolation);

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  // With voice isolation enabled, it is routed through VoiceIsolationHandler
  // (pass-through).
  EXPECT_CALL(deliver_callback_, Run(_, _, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time, std::optional<double> volume,
                    const media::AudioGlitchInfo& glitch_info) {
        EXPECT_EQ(processed_bus.channels(), output_params_.channels());
        EXPECT_EQ(processed_bus.frames(), output_params_.frames_per_buffer());
      });

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                media::AudioGlitchInfo());

  handler->StopProcessing();
}
#endif

TEST_F(AudioProcessorHandlerTest, AsynchronousProcessingWithFifo) {
  media::AudioProcessingSettings settings;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation=*/nullptr);

  handler->StartProcessing();
  EXPECT_FALSE(HasVoiceIsolationHandler(*handler));

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  // With echo cancellation, processing happens asynchronously on the FIFO
  // thread.
  EXPECT_CALL(deliver_callback_, Run(_, _, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time, std::optional<double> volume,
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
TEST_F(AudioProcessorHandlerTest,
       AsynchronousProcessingWithFifoAndVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.voice_isolation = true;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      GetVoiceIsolationHandler(output_params_, deliver_callback_.Get()));

  handler->StartProcessing();
  EXPECT_EQ(HasVoiceIsolationHandler(*handler), settings.voice_isolation);

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
  // With echo cancellation, processing happens asynchronously on the FIFO
  // thread. With voice isolation enabled, it is then routed through voice
  // isolation.
  EXPECT_CALL(deliver_callback_, Run(_, _, _, _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time, std::optional<double> volume,
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
#endif

TEST_F(AudioProcessorHandlerTest, GlitchInfoAccumulation) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation=*/nullptr);

  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  media::AudioGlitchInfo glitch_info1{.duration = base::Milliseconds(10),
                                      .count = 2};
  media::AudioGlitchInfo glitch_info2{.duration = base::Milliseconds(5),
                                      .count = 1};

  EXPECT_CALL(deliver_callback_, Run(_, _, _, glitch_info1)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info1);

  EXPECT_CALL(deliver_callback_, Run(_, _, _, glitch_info2)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info2);

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest, GlitchInfoAccumulationWithFifo) {
  media::AudioProcessingSettings settings;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation=*/nullptr);

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
  EXPECT_CALL(deliver_callback_, Run(_, _, _, glitch_info1)).WillOnce([&]() {
    run_loop1.Quit();
  });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info1);
  run_loop1.Run();

  base::RunLoop run_loop2;
  EXPECT_CALL(deliver_callback_, Run(_, _, _, glitch_info2)).WillOnce([&]() {
    run_loop2.Quit();
  });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info2);
  run_loop2.Run();

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest, VolumePropagation) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation=*/nullptr);

  handler->StartProcessing();

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  double expected_volume = 0.789;
  EXPECT_CALL(deliver_callback_, Run(_, _, Eq(std::nullopt), _)).Times(1);

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                expected_volume, media::AudioGlitchInfo());

  handler->StopProcessing();
}

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
TEST_F(AudioProcessorHandlerTest, GlitchInfoAccumulationWithVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = false;
  settings.voice_isolation = true;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      GetVoiceIsolationHandler(output_params_, deliver_callback_.Get()));

  handler->StartProcessing();
  EXPECT_EQ(HasVoiceIsolationHandler(*handler), settings.voice_isolation);

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  media::AudioGlitchInfo glitch_info1{.duration = base::Milliseconds(10),
                                      .count = 2};
  media::AudioGlitchInfo glitch_info2{.duration = base::Milliseconds(5),
                                      .count = 1};

  EXPECT_CALL(deliver_callback_, Run(_, _, _, glitch_info1)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info1);

  EXPECT_CALL(deliver_callback_, Run(_, _, _, glitch_info2)).Times(1);
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info2);

  handler->StopProcessing();
}

TEST_F(AudioProcessorHandlerTest,
       GlitchInfoAccumulationWithFifoAndVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.voice_isolation = true;

  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      GetVoiceIsolationHandler(output_params_, deliver_callback_.Get()));

  handler->StartProcessing();
  EXPECT_EQ(HasVoiceIsolationHandler(*handler), settings.voice_isolation);

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  media::AudioGlitchInfo glitch_info1{.duration = base::Milliseconds(10),
                                      .count = 2};
  media::AudioGlitchInfo glitch_info2{.duration = base::Milliseconds(5),
                                      .count = 1};

  base::RunLoop run_loop1;
  EXPECT_CALL(deliver_callback_, Run(_, _, _, glitch_info1)).WillOnce([&]() {
    run_loop1.Quit();
  });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info1);
  run_loop1.Run();

  base::RunLoop run_loop2;
  EXPECT_CALL(deliver_callback_, Run(_, _, _, glitch_info2)).WillOnce([&]() {
    run_loop2.Quit();
  });
  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(), 1.0,
                                glitch_info2);
  run_loop2.Run();

  handler->StopProcessing();
}
#endif

}  // namespace

}  // namespace audio
