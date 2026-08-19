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
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/ml_model_handle.h"
#include "media/webrtc/voice_isolation/mock_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
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
  base::MockCallback<AudioProcessorHandler::ReferenceStreamErrorCallback>
      error_callback_;

  bool HasVoiceIsolationHandler(const AudioProcessorHandler& handler) {
    return handler.voice_isolation_handler_ != nullptr;
  }

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  static std::unique_ptr<VoiceIsolationHandler>
  CreateVoiceIsolationHandlerWithMock(
      std::unique_ptr<media::MockVoiceIsolation> mock_voice_isolation,
      const media::AudioParameters& output_params,
      VoiceIsolationHandler::DeliverProcessedAudioCallback callback) {
    // Pass-through.
    ON_CALL(*mock_voice_isolation, ProcessAudio(_, _))
        .WillByDefault([](const media::AudioBus& input,
                          media::AudioBus& output) { input.CopyTo(&output); });
    return VoiceIsolationHandler::CreateForTesting(
        std::move(mock_voice_isolation), output_params, callback);
  }
#endif
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
#endif

TEST_F(AudioProcessorHandlerTest, ProcessingWithoutVoiceIsolationHandler) {
  media::AudioProcessingSettings settings;
  mojo::PendingRemote<media::mojom::AudioProcessorControls> controls_remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      deliver_callback_.Get(), error_callback_.Get(),
      controls_remote.InitWithNewPipeAndPassReceiver(),
      /*aecdump_recording_manager=*/nullptr,
      /*ml_model_manager=*/nullptr,
      /*voice_isolation_handler=*/nullptr);

  handler->StartProcessing();
  EXPECT_FALSE(HasVoiceIsolationHandler(*handler));

  auto input_bus = media::AudioBus::Create(input_params_);
  input_bus->Zero();

  base::RunLoop run_loop;
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
      base::NullCallback(), error_callback_.Get(),
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

TEST_F(AudioProcessorHandlerTest,
       CallingSetVoiceIsolationWhileProcessingTogglesVoiceIsolation) {
  media::AudioProcessingSettings settings;
  settings.voice_isolation = true;

  auto mock_component = std::make_unique<media::MockVoiceIsolation>();
  media::MockVoiceIsolation* voice_isolation_mock_ptr = mock_component.get();

  mojo::Remote<media::mojom::AudioProcessorControls> remote;
  auto handler = std::make_unique<AudioProcessorHandler>(
      settings, input_params_, output_params_, log_callback_.Get(),
      base::NullCallback(), error_callback_.Get(),
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
    EXPECT_CALL(deliver_callback_, Run(_, _, _, _))
        .WillOnce([&](const media::AudioBus& processed_bus,
                      base::TimeTicks capture_time,
                      std::optional<double> volume,
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

  // With voice isolation disabled, the mock component should not be called.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*voice_isolation_mock_ptr, ProcessAudio(_, _)).Times(0);
    EXPECT_CALL(deliver_callback_, Run(_, _, _, _))
        .WillOnce([&](const media::AudioBus& processed_bus,
                      base::TimeTicks capture_time,
                      std::optional<double> volume,
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
    EXPECT_CALL(deliver_callback_, Run(_, _, _, _))
        .WillOnce([&](const media::AudioBus& processed_bus,
                      base::TimeTicks capture_time,
                      std::optional<double> volume,
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
      deliver_callback_.Get(), error_callback_.Get(),
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
      deliver_callback_.Get(), error_callback_.Get(),
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
  // The callback volume parameter is only set if the AGC recommends a volume
  // adjustment. Since the input is silent, the AGC recommends no change,
  // and we expect std::nullopt.
  EXPECT_CALL(deliver_callback_, Run(_, _, Eq(std::nullopt), _))
      .WillOnce([&](const media::AudioBus& processed_bus,
                    base::TimeTicks capture_time, std::optional<double> volume,
                    const media::AudioGlitchInfo& glitch_info) {
        run_loop.Quit();
      });

  handler->ProcessCapturedAudio(*input_bus, base::TimeTicks::Now(),
                                volume, media::AudioGlitchInfo());
  run_loop.Run();

  handler->StopProcessing();
}

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
      base::NullCallback(), error_callback_.Get(),
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
  EXPECT_TRUE(handler);
}
#endif

}  // namespace

}  // namespace audio
