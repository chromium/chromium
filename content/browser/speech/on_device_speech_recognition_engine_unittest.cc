// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "content/browser/speech/on_device_speech_recognition_engine_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/audio_parameters.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

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

}  // namespace content
