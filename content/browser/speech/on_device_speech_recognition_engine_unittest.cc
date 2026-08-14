// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-test-utils.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "content/browser/speech/on_device_speech_recognition_engine_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/audio_parameters.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-test-utils.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class FakeSession
    : public on_device_model::mojom::SessionInterceptorForTesting {
 public:
  on_device_model::mojom::Session* GetForwardingInterface() override {
    return nullptr;
  }
  void AsrStream(
      on_device_model::mojom::AsrStreamOptionsPtr options,
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> stream,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder)
      override {
    last_options_ = std::move(options);
    stream_ = std::move(stream);
    responder_ = std::move(responder);
  }
  on_device_model::mojom::AsrStreamOptionsPtr last_options_;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> stream_;
  mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder_;
  mojo::Receiver<on_device_model::mojom::Session> receiver_{this};
};

class FakeModelSolution
    : public optimization_guide::mojom::ModelSolutionInterceptorForTesting {
 public:
  explicit FakeModelSolution(FakeSession* session) : session_(session) {}
  optimization_guide::mojom::ModelSolution* GetForwardingInterface() override {
    return nullptr;
  }
  void CreateSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> session,
      on_device_model::mojom::SessionParamsPtr params) override {
    session_->receiver_.Bind(std::move(session));
  }
  raw_ptr<FakeSession> session_;
  mojo::Receiver<optimization_guide::mojom::ModelSolution> receiver_{this};
};

class MockSpeechRecognitionEngineDelegate
    : public SpeechRecognitionEngine::Delegate {
 public:
  MOCK_METHOD(void,
              OnSpeechRecognitionEngineResults,
              (const std::vector<media::mojom::WebSpeechRecognitionResultPtr>&),
              (override));
  MOCK_METHOD(void, OnSpeechRecognitionEngineEndOfUtterance, (), (override));
  MOCK_METHOD(void,
              OnSpeechRecognitionEngineError,
              (const media::mojom::SpeechRecognitionError&),
              (override));
};

TEST(OnDeviceSpeechRecognitionEngine, ConvertAccumulatedAudioData) {
  // A BrowserTaskEnvironment is necessary because the
  // OnDeviceSpeechRecognitionEngine post tasks.
  BrowserTaskEnvironment task_environment;

  // Verifies that ConvertAccumulatedAudioData correctly normalizes audio
  // samples.
  OnDeviceSpeechRecognitionEngine engine(SpeechRecognitionSessionConfig{});

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(), 16000, 160);
  engine.SetAudioParameters(params);

  // The int16_t values 16384 and -32768 should normalize to 0.5 and -1.0.
  engine.accumulated_audio_data_ = {16384, -32768};

  on_device_model::mojom::AudioDataPtr converted_audio_data =
      engine.ConvertAccumulatedAudioData();

  EXPECT_THAT(converted_audio_data->data,
              testing::Pointwise(testing::FloatEq(), {0.5f, -1.0f}));
}

TEST(OnDeviceSpeechRecognitionEngine, ShutdownRace) {
  // A BrowserTaskEnvironment is necessary because the
  // OnDeviceSpeechRecognitionEngine post tasks.
  BrowserTaskEnvironment task_environment;

  {
    OnDeviceSpeechRecognitionEngine engine(SpeechRecognitionSessionConfig{});
    // Constructor posts CreateModelClient to the UI thread.
  }
  // Destruction of engine posts Deleter to the UI thread.

  // Run all tasks to ensure that the posted CreateModelClient task (which
  // uses the core) and the Deleter task (which destroys the core) do not
  // cause a use-after-free.
  task_environment.RunUntilIdle();
}

TEST(OnDeviceSpeechRecognitionEngine, Reinitialization) {
  // A BrowserTaskEnvironment is necessary because the
  // OnDeviceSpeechRecognitionEngine post tasks.
  BrowserTaskEnvironment task_environment;

  OnDeviceSpeechRecognitionEngine engine(SpeechRecognitionSessionConfig{});

  // Mock a ModelClient to allow TryCreateSession to proceed.
  mojo::Remote<optimization_guide::mojom::ModelSolution> solution_remote;
  auto solution_receiver = solution_remote.BindNewPipeAndPassReceiver();

  auto config = optimization_guide::mojom::ModelSolutionConfig::New();
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig feature_config;
  feature_config.set_feature(
      optimization_guide::proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  config->feature_config = mojo_base::ProtoWrapper(feature_config);
  config->text_safety_config = mojo_base::ProtoWrapper(
      optimization_guide::proto::FeatureTextSafetyConfiguration());
  config->model_versions = mojo_base::ProtoWrapper(
      optimization_guide::proto::OnDeviceModelVersions());

  std::unique_ptr<optimization_guide::ModelClient> ui_model_client;

  // Manually set the model client in Core on the UI thread.
  engine.core_.PostTaskWithThisObject(
      base::BindLambdaForTesting([&](OnDeviceSpeechRecognitionEngine::Core* core) {
        ui_model_client = std::make_unique<optimization_guide::ModelClient>(
            solution_remote.Unbind(), std::move(config),
            on_device_model::Capabilities());
        core->model_client_ = ui_model_client->GetWeakPtr();
      }));
  task_environment.RunUntilIdle();

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(), 16000, 160);

  // Calling SetAudioParameters should cause session_created_ to be set to true.
  base::test::TestFuture<bool> session_created_future;
  engine.core_.PostTaskWithThisObject(
      base::BindLambdaForTesting([&](OnDeviceSpeechRecognitionEngine::Core* core) {
        session_created_future.SetValue(core->session_created_);
      }));
  EXPECT_FALSE(session_created_future.Get());

  engine.SetAudioParameters(params);

  // Run tasks so Core::SetAudioParameters and Core::TryCreateSession run.
  task_environment.RunUntilIdle();

  base::test::TestFuture<bool> session_created_future_2;
  engine.core_.PostTaskWithThisObject(
      base::BindLambdaForTesting([&](OnDeviceSpeechRecognitionEngine::Core* core) {
        session_created_future_2.SetValue(core->session_created_);
      }));
  EXPECT_TRUE(session_created_future_2.Get());

  // Calling it again should still have it as true (because it was set).
  engine.SetAudioParameters(params);
  task_environment.RunUntilIdle();

  base::test::TestFuture<bool> session_created_future_3;
  engine.core_.PostTaskWithThisObject(
      base::BindLambdaForTesting([&](OnDeviceSpeechRecognitionEngine::Core* core) {
        session_created_future_3.SetValue(core->session_created_);
      }));
  EXPECT_TRUE(session_created_future_3.Get());

  // Clean up on the UI thread before the test finishes.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&]() { ui_model_client.reset(); }));
  task_environment.RunUntilIdle();
}

TEST(OnDeviceSpeechRecognitionEngine, AudioChunksEndedDispatchesEmptyResult) {
  BrowserTaskEnvironment task_environment;
  MockSpeechRecognitionEngineDelegate delegate;

  {
    OnDeviceSpeechRecognitionEngine engine(SpeechRecognitionSessionConfig{});
    engine.set_delegate(&delegate);

    EXPECT_CALL(delegate, OnSpeechRecognitionEngineResults(testing::IsEmpty()))
        .Times(1);

    engine.AudioChunksEnded();
  }

  task_environment.RunUntilIdle();
}

TEST(OnDeviceSpeechRecognitionEngine, LanguagePropagation) {
  BrowserTaskEnvironment task_environment;

  SpeechRecognitionSessionConfig session_config;
  session_config.language = "en-US";
  OnDeviceSpeechRecognitionEngine engine(session_config);

  FakeSession fake_session;
  FakeModelSolution fake_solution(&fake_session);

  mojo::Remote<optimization_guide::mojom::ModelSolution> solution_remote;
  fake_solution.receiver_.Bind(solution_remote.BindNewPipeAndPassReceiver());

  auto config = optimization_guide::mojom::ModelSolutionConfig::New();
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig feature_config;
  feature_config.set_feature(
      optimization_guide::proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  config->feature_config = mojo_base::ProtoWrapper(feature_config);
  config->text_safety_config = mojo_base::ProtoWrapper(
      optimization_guide::proto::FeatureTextSafetyConfiguration());
  config->model_versions = mojo_base::ProtoWrapper(
      optimization_guide::proto::OnDeviceModelVersions());

  std::unique_ptr<optimization_guide::ModelClient> ui_model_client;

  engine.core_.PostTaskWithThisObject(base::BindLambdaForTesting(
      [&](OnDeviceSpeechRecognitionEngine::Core* core) {
        ui_model_client = std::make_unique<optimization_guide::ModelClient>(
            solution_remote.Unbind(), std::move(config),
            on_device_model::Capabilities());
        core->model_client_ = ui_model_client->GetWeakPtr();
      }));
  task_environment.RunUntilIdle();

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(), 16000, 160);
  engine.SetAudioParameters(params);
  task_environment.RunUntilIdle();

  ASSERT_TRUE(fake_session.last_options_);
  EXPECT_EQ(fake_session.last_options_->language, "en-US");

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&]() { ui_model_client.reset(); }));
  task_environment.RunUntilIdle();
}

TEST(OnDeviceSpeechRecognitionEngine, EmptyLanguagePropagation) {
  BrowserTaskEnvironment task_environment;

  SpeechRecognitionSessionConfig session_config;
  session_config.language = "";
  OnDeviceSpeechRecognitionEngine engine(session_config);

  FakeSession fake_session;
  FakeModelSolution fake_solution(&fake_session);

  mojo::Remote<optimization_guide::mojom::ModelSolution> solution_remote;
  fake_solution.receiver_.Bind(solution_remote.BindNewPipeAndPassReceiver());

  auto config = optimization_guide::mojom::ModelSolutionConfig::New();
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig feature_config;
  feature_config.set_feature(
      optimization_guide::proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  config->feature_config = mojo_base::ProtoWrapper(feature_config);
  config->text_safety_config = mojo_base::ProtoWrapper(
      optimization_guide::proto::FeatureTextSafetyConfiguration());
  config->model_versions = mojo_base::ProtoWrapper(
      optimization_guide::proto::OnDeviceModelVersions());

  std::unique_ptr<optimization_guide::ModelClient> ui_model_client;

  engine.core_.PostTaskWithThisObject(base::BindLambdaForTesting(
      [&](OnDeviceSpeechRecognitionEngine::Core* core) {
        ui_model_client = std::make_unique<optimization_guide::ModelClient>(
            solution_remote.Unbind(), std::move(config),
            on_device_model::Capabilities());
        core->model_client_ = ui_model_client->GetWeakPtr();
      }));
  task_environment.RunUntilIdle();

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(), 16000, 160);
  engine.SetAudioParameters(params);
  task_environment.RunUntilIdle();

  ASSERT_TRUE(fake_session.last_options_);
  EXPECT_FALSE(fake_session.last_options_->language.has_value());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&]() { ui_model_client.reset(); }));
  task_environment.RunUntilIdle();
}

TEST(OnDeviceSpeechRecognitionEngine, TakeAudioChunkStereo) {
  BrowserTaskEnvironment task_environment;
  OnDeviceSpeechRecognitionEngine engine(SpeechRecognitionSessionConfig{});

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(), 16000,
                                160);
  engine.SetAudioParameters(params);

  // 4 int16_t samples = 2 stereo frames (L1, R1, L2, R2).
  constexpr std::array<int16_t, 4> raw_pcm = {1000, 3000, -2000, 4000};
  auto chunk = base::MakeRefCounted<AudioChunk>(base::as_byte_span(raw_pcm),
                                                sizeof(int16_t));

  engine.TakeAudioChunk(*chunk);

  // Average for frame 1: (1000 + 3000) / 2 = 2000
  // Average for frame 2: (-2000 + 4000) / 2 = 1000
  ASSERT_EQ(engine.accumulated_audio_data_.size(), 2u);
  EXPECT_EQ(engine.accumulated_audio_data_[0], 2000);
  EXPECT_EQ(engine.accumulated_audio_data_[1], 1000);
}

TEST(OnDeviceSpeechRecognitionEngine, NullDelegateOnResponseAndDisconnect) {
  BrowserTaskEnvironment task_environment;
  OnDeviceSpeechRecognitionEngine engine(SpeechRecognitionSessionConfig{});

  // Calling OnResponse with nullptr delegate_ should not crash.
  std::vector<on_device_model::mojom::SpeechRecognitionResultPtr> results;
  engine.OnResponse(std::move(results));

  // AudioChunksEnded with nullptr delegate_ should not crash.
  engine.AudioChunksEnded();
}

}  // namespace content
