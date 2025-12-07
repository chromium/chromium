// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/on_device_speech_recognition_engine_impl.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/common/content_client.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"

namespace content {

OnDeviceSpeechRecognitionEngine::Core::Core(
    on_device_model::mojom::AsrStreamResponder* responder)
    : asr_stream_responder(responder) {}
OnDeviceSpeechRecognitionEngine::Core::~Core() = default;

void OnDeviceSpeechRecognitionEngine::Core::Deleter::operator()(
    Core* core) const {
  // This class is created on the UI thread and thus must be deleted on the UI
  // thread.
  if (!GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, core)) {
    delete core;
  }
}

OnDeviceSpeechRecognitionEngine::OnDeviceSpeechRecognitionEngine(
    const SpeechRecognitionSessionConfig& config)
    : ui_task_runner_(GetUIThreadTaskRunner({})),
      io_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      config_(config),
      core_(new Core(this)) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceSpeechRecognitionEngine::CreateModelClientOnUI,
                     weak_factory_.GetWeakPtr(),
                     config_.initial_context.render_process_id,
                     config_.initial_context.render_frame_id));
}

OnDeviceSpeechRecognitionEngine::~OnDeviceSpeechRecognitionEngine() = default;

void OnDeviceSpeechRecognitionEngine::StartRecognition() {}

void OnDeviceSpeechRecognitionEngine::EndRecognition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  core_.reset();
  asr_stream_.reset();
}

void OnDeviceSpeechRecognitionEngine::SetAudioParameters(
    media::AudioParameters audio_parameters) {
  SpeechRecognitionEngine::SetAudioParameters(audio_parameters);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceSpeechRecognitionEngine::CreateSessionOnUI,
                     weak_factory_.GetWeakPtr()));
}

void OnDeviceSpeechRecognitionEngine::TakeAudioChunk(const AudioChunk& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  const size_t num_samples = data.NumSamples() * audio_parameters_.channels();
  std::vector<int16_t> pcm_data_vector(num_samples);
  auto source_bytes = base::as_bytes(base::span(data.AsString()));
  auto dest_bytes = base::as_writable_bytes(base::span(pcm_data_vector));
  CHECK_EQ(source_bytes.size(), dest_bytes.size());
  dest_bytes.copy_from(source_bytes);
  accumulated_audio_data_.insert(accumulated_audio_data_.end(),
                                 pcm_data_vector.begin(),
                                 pcm_data_vector.end());

  // The duration of audio to accumulate before sending to the on-device
  // service.
  constexpr base::TimeDelta kAudioBufferDuration = base::Seconds(2);

  if (media::AudioTimestampHelper::FramesToTime(
          accumulated_audio_data_.size(), audio_parameters_.sample_rate()) >=
      kAudioBufferDuration) {
    if (asr_stream_.is_bound()) {
      asr_stream_->AddAudioChunk(ConvertAccumulatedAudioData());
    }
  }
}

void OnDeviceSpeechRecognitionEngine::UpdateRecognitionContext(
    const media::SpeechRecognitionRecognitionContext& recognition_context) {
  // TODO(crbug.com/446260680): Implement biasing support with NanoV3.
}

void OnDeviceSpeechRecognitionEngine::OnResponse(
    std::vector<on_device_model::mojom::SpeechRecognitionResultPtr> result) {
  std::vector<media::mojom::WebSpeechRecognitionResultPtr> recognition_results;
  for (const auto& r : result) {
    auto web_speech_result = media::mojom::WebSpeechRecognitionResult::New();
    web_speech_result->is_provisional = !r->is_final;

    constexpr float kSpeechRecognitionConfidence = 1.0f;
    web_speech_result->hypotheses.emplace_back(
        media::mojom::SpeechRecognitionHypothesis::New(
            base::UTF8ToUTF16(r->transcript), kSpeechRecognitionConfidence));
    recognition_results.push_back(std::move(web_speech_result));
  }
  delegate_->OnSpeechRecognitionEngineResults(std::move(recognition_results));
}

void OnDeviceSpeechRecognitionEngine::AudioChunksEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  core_.reset();
  asr_stream_.reset();
}

int OnDeviceSpeechRecognitionEngine::GetDesiredAudioChunkDurationMs() const {
  constexpr int kAudioPacketIntervalMs = 100;
  return kAudioPacketIntervalMs;
}

void OnDeviceSpeechRecognitionEngine::CreateModelClientOnUI(
    int render_process_id,
    int render_frame_id) {
  RenderFrameHost* rfh =
      RenderFrameHost::FromID(config_.initial_context.render_process_id,
                              config_.initial_context.render_frame_id);
  if (!rfh) {
    return;
  }

  core_->model_broker_client =
      GetContentClient()->browser()->CreateModelBrokerClient(
          rfh->GetBrowserContext());

  if (core_->model_broker_client) {
    core_->model_broker_client
        ->GetSubscriber(optimization_guide::mojom::OnDeviceFeature::kPromptApi)
        .WaitForClient(base::BindOnce(
            &OnDeviceSpeechRecognitionEngine::OnModelClientAvailable,
            weak_factory_.GetWeakPtr()));
  }
}

void OnDeviceSpeechRecognitionEngine::OnModelClientAvailable(
    base::WeakPtr<optimization_guide::ModelClient> client) {
  core_->model_client = client;
  CreateSessionOnUI();
}

void OnDeviceSpeechRecognitionEngine::CreateSessionOnUI() {
  if (!core_ || !core_->model_client || !audio_parameters_.IsValid() ||
      session_created_) {
    return;
  }

  session_created_ = true;

  auto params = on_device_model::mojom::SessionParams::New();
  params->capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
  core_->model_client->solution().CreateSession(
      core_->session.BindNewPipeAndPassReceiver(), std::move(params));

  auto asr_options = on_device_model::mojom::AsrStreamOptions::New();
  asr_options->sample_rate_hz = audio_parameters_.sample_rate();

  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> remote;
  core_->session->AsrStream(
      std::move(asr_options), remote.InitWithNewPipeAndPassReceiver(),
      core_->asr_stream_responder.BindNewPipeAndPassRemote());

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceSpeechRecognitionEngine::OnAsrStreamCreated,
                     weak_factory_.GetWeakPtr(), std::move(remote)));
}

void OnDeviceSpeechRecognitionEngine::OnAsrStreamCreated(
    mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  asr_stream_.Bind(std::move(remote));
}

void OnDeviceSpeechRecognitionEngine::OnRecognizerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  EndRecognition();
}

on_device_model::mojom::AudioDataPtr
OnDeviceSpeechRecognitionEngine::ConvertAccumulatedAudioData() {
  CHECK_EQ(audio_parameters_.channels(), 1);

  auto signed_buffer = on_device_model::mojom::AudioData::New();
  signed_buffer->channel_count = audio_parameters_.channels();
  signed_buffer->sample_rate = audio_parameters_.sample_rate();
  signed_buffer->frame_count = accumulated_audio_data_.size();

  // Normalization factor for converting int16_t audio samples to float.
  constexpr float kInt16ToFloatNormalizer = 32768.0f;
  signed_buffer->data = base::ToVector(accumulated_audio_data_, [](int16_t s) {
    return s / kInt16ToFloatNormalizer;
  });

  accumulated_audio_data_.clear();
  return signed_buffer;
}

}  // namespace content
