// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/on_device_speech_recognition_engine_impl.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/common/content_client.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"

namespace content {

namespace {
constexpr char kWebSpeechTinyGemmaDuration[] =
    "Accessibility.WebSpeech.TinyGemma.Duration";
constexpr char kWebSpeechGeminiNanoDuration[] =
    "Accessibility.WebSpeech.GeminiNano.Duration";
}  // namespace

OnDeviceSpeechRecognitionEngine::Core::Core(
    StreamCreatedCallback on_stream_created_callback)
    : on_stream_created_callback_(std::move(on_stream_created_callback)) {}
OnDeviceSpeechRecognitionEngine::Core::~Core() = default;

OnDeviceSpeechRecognitionEngine::OnDeviceSpeechRecognitionEngine(
    const SpeechRecognitionSessionConfig& config)
    : config_(config) {
  core_ = base::SequenceBound<Core>(
      content::GetUIThreadTaskRunner({}),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &OnDeviceSpeechRecognitionEngine::OnAsrStreamCreated,
          weak_factory_.GetWeakPtr())));
  core_.AsyncCall(&Core::CreateModelClient)
      .WithArgs(config_.initial_context.global_id, config_.quality);
}

OnDeviceSpeechRecognitionEngine::~OnDeviceSpeechRecognitionEngine() = default;

void OnDeviceSpeechRecognitionEngine::StartRecognition() {
  audio_duration_ = base::TimeDelta();
}

void OnDeviceSpeechRecognitionEngine::EndRecognition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  const bool use_gemini_nano =
      base::FeatureList::IsEnabled(media::kOnDeviceWebSpeechGeminiNano) &&
      config_.quality == media::mojom::SpeechRecognitionQuality::kConversation;
  if (use_gemini_nano) {
    base::UmaHistogramLongTimes100(kWebSpeechGeminiNanoDuration,
                                   audio_duration_);
  } else {
    base::UmaHistogramLongTimes100(kWebSpeechTinyGemmaDuration,
                                   audio_duration_);
  }

  core_.Reset();
  asr_stream_.reset();
  asr_stream_responder_.reset();
}

void OnDeviceSpeechRecognitionEngine::SetAudioParameters(
    media::AudioParameters audio_parameters) {
  SpeechRecognitionEngine::SetAudioParameters(audio_parameters);

  core_.AsyncCall(&Core::SetAudioParameters)
      .WithArgs(audio_parameters.sample_rate());
}

void OnDeviceSpeechRecognitionEngine::TakeAudioChunk(const AudioChunk& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  audio_duration_ += media::AudioTimestampHelper::FramesToTime(
      data.NumSamples(), audio_parameters_.sample_rate());

  const size_t num_samples = data.NumSamples() * audio_parameters_.channels();
  std::vector<int16_t> pcm_data_vector(num_samples);
  auto source_bytes = data.data();
  auto dest_bytes = base::as_writable_bytes(base::span(pcm_data_vector));
  CHECK_EQ(source_bytes.size(), dest_bytes.size());
  dest_bytes.copy_from(source_bytes);
  accumulated_audio_data_.insert(accumulated_audio_data_.end(),
                                 pcm_data_vector.begin(),
                                 pcm_data_vector.end());

  if (asr_stream_.is_bound()) {
    asr_stream_->AddAudioChunk(ConvertAccumulatedAudioData());
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
  core_.Reset();
  asr_stream_.reset();
  asr_stream_responder_.reset();
}

int OnDeviceSpeechRecognitionEngine::GetDesiredAudioChunkDurationMs() const {
  constexpr int kAudioPacketIntervalMs = 100;
  return kAudioPacketIntervalMs;
}

void OnDeviceSpeechRecognitionEngine::Core::CreateModelClient(
    GlobalRenderFrameHostId global_id,
    media::mojom::SpeechRecognitionQuality quality) {
  RenderFrameHost* rfh = RenderFrameHost::FromID(global_id);
  if (!rfh) {
    return;
  }

  model_broker_client_ = GetContentClient()->browser()->CreateModelBrokerClient(
      rfh->GetBrowserContext());

  optimization_guide::mojom::OnDeviceFeature feature =
      quality == media::mojom::SpeechRecognitionQuality::kDictation
          ? optimization_guide::mojom::OnDeviceFeature::
                kSpeechRecognitionSmallExpertModel
          : optimization_guide::mojom::OnDeviceFeature::
                kOnDeviceSpeechRecognition;

  if (model_broker_client_) {
    model_broker_client_->RequestAssetsFor(feature);
    model_broker_client_->GetSubscriber(feature).WaitForClient(base::BindOnce(
        &Core::OnModelClientAvailable, weak_factory_.GetWeakPtr()));
  }
}

void OnDeviceSpeechRecognitionEngine::Core::OnModelClientAvailable(
    base::WeakPtr<optimization_guide::ModelClient> client) {
  model_client_ = client;
  TryCreateSession();
}

void OnDeviceSpeechRecognitionEngine::Core::SetAudioParameters(
    int sample_rate_hz) {
  sample_rate_hz_ = sample_rate_hz;
  TryCreateSession();
}

void OnDeviceSpeechRecognitionEngine::Core::TryCreateSession() {
  if (!model_client_ || !sample_rate_hz_.has_value() || session_created_) {
    return;
  }

  session_created_ = true;

  auto params = on_device_model::mojom::SessionParams::New();
  params->capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
  model_client_->solution().CreateSession(session_.BindNewPipeAndPassReceiver(),
                                          std::move(params));

  auto asr_options = on_device_model::mojom::AsrStreamOptions::New();
  asr_options->sample_rate_hz = *sample_rate_hz_;

  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  session_->AsrStream(std::move(asr_options),
                      asr_stream.InitWithNewPipeAndPassReceiver(),
                      asr_stream_responder.InitWithNewPipeAndPassRemote());

  if (on_stream_created_callback_) {
    std::move(on_stream_created_callback_)
        .Run(std::move(asr_stream), std::move(asr_stream_responder));
  }
}

void OnDeviceSpeechRecognitionEngine::OnAsrStreamCreated(
    mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream,
    mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
        asr_stream_responder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  asr_stream_.Bind(std::move(asr_stream));
  asr_stream_responder_.Bind(std::move(asr_stream_responder));
  asr_stream_responder_.set_disconnect_with_reason_handler(base::BindOnce(
      &OnDeviceSpeechRecognitionEngine::OnResponderDisconnectedWithReason,
      weak_factory_.GetWeakPtr()));
}

void OnDeviceSpeechRecognitionEngine::OnRecognizerDisconnected() {
  OnResponderDisconnectedWithReason(0, "");
}

void OnDeviceSpeechRecognitionEngine::OnResponderDisconnectedWithReason(
    uint32_t custom_reason,
    const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  media::mojom::SpeechRecognitionError error;
  error.code = media::mojom::SpeechRecognitionErrorCode::kServiceNotAllowed;
  error.details = media::mojom::SpeechAudioErrorDetails::kNone;
  delegate_->OnSpeechRecognitionEngineError(error);
  EndRecognition();
}

on_device_model::mojom::AudioDataPtr
OnDeviceSpeechRecognitionEngine::ConvertAccumulatedAudioData() {
  CHECK_EQ(audio_parameters_.channels(), 1);

  auto signed_buffer = on_device_model::mojom::AudioData::New();
  signed_buffer->channel_count = audio_parameters_.channels();
  signed_buffer->sample_rate = audio_parameters_.sample_rate();
  signed_buffer->frame_count =
      base::checked_cast<int32_t>(accumulated_audio_data_.size());

  // Normalization factor for converting int16_t audio samples to float.
  constexpr float kInt16ToFloatNormalizer = 32768.0f;
  signed_buffer->data = base::ToVector(accumulated_audio_data_, [](int16_t s) {
    return s / kInt16ToFloatNormalizer;
  });

  accumulated_audio_data_.clear();
  return signed_buffer;
}

}  // namespace content
