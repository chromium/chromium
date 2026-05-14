// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_ENGINE_IMPL_H_
#define CONTENT_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_ENGINE_IMPL_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/common/content_export.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {
class ModelBrokerClient;
}  // namespace optimization_guide

namespace content {
class BrowserContext;
struct GlobalRenderFrameHostId;

class CONTENT_EXPORT OnDeviceSpeechRecognitionEngine
    : public SpeechRecognitionEngine,
      public on_device_model::mojom::AsrStreamResponder {
 public:
  explicit OnDeviceSpeechRecognitionEngine(
      const SpeechRecognitionSessionConfig& config);
  ~OnDeviceSpeechRecognitionEngine() override;
  OnDeviceSpeechRecognitionEngine(const OnDeviceSpeechRecognitionEngine&) =
      delete;
  OnDeviceSpeechRecognitionEngine& operator=(
      const OnDeviceSpeechRecognitionEngine&) = delete;

  // SpeechRecognitionEngine:
  void StartRecognition() override;
  void EndRecognition() override;
  void TakeAudioChunk(const AudioChunk& data) override;
  void SetAudioParameters(media::AudioParameters audio_parameters) override;
  void AudioChunksEnded() override;
  void UpdateRecognitionContext(
      const media::SpeechRecognitionRecognitionContext& recognition_context)
      override;
  int GetDesiredAudioChunkDurationMs() const override;

  // on_device_model::mojom::AsrStreamResponder:
  void OnResponse(
      std::vector<on_device_model::mojom::SpeechRecognitionResultPtr> result)
      override;

  // Helper class to manage lifetimes of objects that live on the UI thread.
  class Core {
   public:
    using StreamCreatedCallback = base::OnceCallback<void(
        mojo::PendingRemote<on_device_model::mojom::AsrStreamInput>,
        mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>)>;

    explicit Core(StreamCreatedCallback on_stream_created_callback);
    ~Core();

    void CreateModelClient(GlobalRenderFrameHostId global_id,
                           media::mojom::SpeechRecognitionQuality quality);
    void SetAudioParameters(int sample_rate_hz);

   private:
    friend class OnDeviceSpeechRecognitionEngineTest;
    FRIEND_TEST(OnDeviceSpeechRecognitionEngine, Reinitialization);

    void OnModelClientAvailable(
        base::WeakPtr<optimization_guide::ModelClient> client);
    void TryCreateSession();

    mojo::Remote<on_device_model::mojom::Session> session_;
    base::WeakPtr<optimization_guide::ModelClient> model_client_;
    std::unique_ptr<optimization_guide::ModelBrokerClient> model_broker_client_;

    std::optional<int> sample_rate_hz_;
    bool session_created_ = false;

    StreamCreatedCallback on_stream_created_callback_;

    base::WeakPtrFactory<Core> weak_factory_{this};
  };

 private:
  friend class OnDeviceSpeechRecognitionEngineTest;
  FRIEND_TEST(OnDeviceSpeechRecognitionEngine, ConvertAccumulatedAudioData);
  FRIEND_TEST(OnDeviceSpeechRecognitionEngine, Reinitialization);

  void OnAsrStreamCreated(
      mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream,
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
          asr_stream_responder);

  void OnRecognizerDisconnected();
  void OnResponderDisconnectedWithReason(uint32_t custom_reason,
                                         const std::string& description);

  on_device_model::mojom::AudioDataPtr ConvertAccumulatedAudioData();

  SpeechRecognitionSessionConfig config_;
  base::SequenceBound<Core> core_;
  mojo::Remote<on_device_model::mojom::AsrStreamInput> asr_stream_;
  mojo::Receiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder_{this};
  std::vector<int16_t> accumulated_audio_data_;

  base::TimeDelta audio_duration_;

  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<OnDeviceSpeechRecognitionEngine> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_ENGINE_IMPL_H_
