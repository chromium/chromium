// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/soda_speech_recognition_engine_impl.h"

#include <string.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

namespace {
// Duration of each audio packet.
constexpr int kAudioPacketIntervalMs = 100;
constexpr float kSpeechRecognitionConfidence = 1.0f;

// Substitute the real instances in browser and unit tests.
SpeechRecognitionManagerDelegate* speech_recognition_mgr_delegate_for_tests =
    nullptr;
}  // namespace

SodaSpeechRecognitionEngineImpl::SodaSpeechRecognitionEngineImpl(
    const SpeechRecognitionSessionConfig& config)
    : config_(config) {
  send_audio_callback_ = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &SodaSpeechRecognitionEngineImpl::SendAudioToSpeechRecognitionService,
      weak_factory_.GetWeakPtr()));

  mark_done_callback_ = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &SodaSpeechRecognitionEngineImpl::MarkDone, weak_factory_.GetWeakPtr()));
}

SodaSpeechRecognitionEngineImpl::~SodaSpeechRecognitionEngineImpl() = default;

bool SodaSpeechRecognitionEngineImpl::Initialize() {
  if (speech_recognition_context_.is_bound()) {
    return true;
  }

  raw_ptr<SpeechRecognitionManagerDelegate> speech_recognition_mgr_delegate;
  if (!speech_recognition_mgr_delegate_for_tests) {
    speech_recognition_mgr_delegate =
        SpeechRecognitionManagerImpl::GetInstance()
            ? SpeechRecognitionManagerImpl::GetInstance()->delegate()
            : nullptr;
  } else {
    speech_recognition_mgr_delegate = speech_recognition_mgr_delegate_for_tests;
  }

  if (!speech_recognition_mgr_delegate) {
    return false;
  }

  // Create a SpeechRecognitionContext and bind it to the current
  // SodaSpeechRecognitionEngineImpl. The SpeechRecognitionContext passes the
  // SpeechRecognitionRecognizer receiver and moves the
  // SpeechRecognitionRecognizerClient. The receiver is in the utility process
  // on Linux/Mac/Windows and in the Ash process on ChromeOS.
  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();
  media::mojom::SpeechRecognitionOptionsPtr options =
      media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->enable_formatting = false;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->skip_continuously_empty_audio = true;

  speech_recognition_context_->BindRecognizer(
      speech_recognition_recognizer_.BindNewPipeAndPassReceiver(),
      speech_recognition_recognizer_client_.BindNewPipeAndPassRemote(),
      std::move(options),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&SodaSpeechRecognitionEngineImpl::OnRecognizerBound,
                         weak_factory_.GetWeakPtr())));

  speech_recognition_mgr_delegate->BindSpeechRecognitionContext(
      std::move(speech_recognition_context_receiver));

  speech_recognition_context_.set_disconnect_handler(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &SodaSpeechRecognitionEngineImpl::OnRecognizerDisconnected,
          weak_factory_.GetWeakPtr())));
  return true;
}

void SodaSpeechRecognitionEngineImpl::StartRecognition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  is_start_recognition_ = true;
}

void SodaSpeechRecognitionEngineImpl::EndRecognition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  is_start_recognition_ = false;
}

void SodaSpeechRecognitionEngineImpl::TakeAudioChunk(const AudioChunk& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!is_start_recognition_) {
    Abort(media::mojom::SpeechRecognitionErrorCode::kNotAllowed);
    return;
  }

  send_audio_callback_.Run(ConvertToAudioDataS16(data));
}

void SodaSpeechRecognitionEngineImpl::AudioChunksEnded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  mark_done_callback_.Run();
}

int SodaSpeechRecognitionEngineImpl::GetDesiredAudioChunkDurationMs() const {
  return kAudioPacketIntervalMs;
}

// media::mojom::SpeechRecognitionRecognizerClient:
void SodaSpeechRecognitionEngineImpl::OnSpeechRecognitionRecognitionEvent(
    const media::SpeechRecognitionResult& recognition_result,
    OnSpeechRecognitionRecognitionEventCallback reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Returning recognition state.
  std::move(reply).Run(is_start_recognition_);

  // Map recognition results.
  std::vector<media::mojom::WebSpeechRecognitionResultPtr> results;
  results.push_back(media::mojom::WebSpeechRecognitionResult::New());
  media::mojom::WebSpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = !recognition_result.is_final;

  media::mojom::SpeechRecognitionHypothesisPtr hypothesis =
      media::mojom::SpeechRecognitionHypothesis::New();
  // TODO(crbug.com/40286514): Hardcode now.
  hypothesis->confidence = kSpeechRecognitionConfidence;
  hypothesis->utterance = base::UTF8ToUTF16(recognition_result.transcription);
  result->hypotheses.push_back(std::move(hypothesis));

  if (!config_.continuous && !result->is_provisional) {
    delegate_->OnSpeechRecognitionEngineEndOfUtterance();
  }

  delegate_->OnSpeechRecognitionEngineResults(results);
}

void SodaSpeechRecognitionEngineImpl::OnSpeechRecognitionError() {
  Abort(media::mojom::SpeechRecognitionErrorCode::kNoSpeech);
}

void SodaSpeechRecognitionEngineImpl::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {}

void SodaSpeechRecognitionEngineImpl::OnSpeechRecognitionStopped() {
  Abort(media::mojom::SpeechRecognitionErrorCode::kAborted);
}

void SodaSpeechRecognitionEngineImpl::
    SetSpeechRecognitionManagerDelegateForTesting(
        SpeechRecognitionManagerDelegate* delegate) {
  speech_recognition_mgr_delegate_for_tests = delegate;
}

void SodaSpeechRecognitionEngineImpl::SetOnReadyCallback(
    base::OnceCallback<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  on_ready_callback_ = std::move(callback);

  if (on_ready_callback_) {
    std::move(on_ready_callback_).Run();
  }
}

void SodaSpeechRecognitionEngineImpl::OnRecognizerBound(
    bool is_multichannel_supported) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (on_ready_callback_) {
    std::move(on_ready_callback_).Run();
  }
}

void SodaSpeechRecognitionEngineImpl::OnRecognizerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  Abort(media::mojom::SpeechRecognitionErrorCode::kAborted);
}

void SodaSpeechRecognitionEngineImpl::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr audio_data) {
  DCHECK(audio_data);
  if (speech_recognition_recognizer_.is_bound()) {
    speech_recognition_recognizer_->SendAudioToSpeechRecognitionService(
        std::move(audio_data));
  }
}

void SodaSpeechRecognitionEngineImpl::MarkDone() {
  if (speech_recognition_recognizer_.is_bound()) {
    speech_recognition_recognizer_->MarkDone();
  }
}

void SodaSpeechRecognitionEngineImpl::Abort(
    media::mojom::SpeechRecognitionErrorCode error_code) {
  DVLOG(1) << "Aborting with error " << error_code;

  if (error_code != media::mojom::SpeechRecognitionErrorCode::kNone) {
    delegate_->OnSpeechRecognitionEngineError(
        media::mojom::SpeechRecognitionError(
            error_code, media::mojom::SpeechAudioErrorDetails::kNone));
  }
}

media::mojom::AudioDataS16Ptr
SodaSpeechRecognitionEngineImpl::ConvertToAudioDataS16(
    const AudioChunk& audio_data) {
  // Only support mono and 2 bytes depth audio format.
  CHECK_EQ(audio_parameters_.channels(), 1);
  CHECK_EQ(audio_data.bytes_per_sample(), 2);

  auto signed_buffer = media::mojom::AudioDataS16::New();
  signed_buffer->channel_count = audio_parameters_.channels();
  signed_buffer->sample_rate = audio_parameters_.sample_rate();
  signed_buffer->frame_count = audio_data.NumSamples();

  signed_buffer->data.resize(audio_data.NumSamples() *
                             audio_parameters_.channels());

  size_t audio_byte_size =
      audio_data.NumSamples() * audio_data.bytes_per_sample();
  memcpy(&signed_buffer->data[0], audio_data.SamplesData16(), audio_byte_size);

  return signed_buffer;
}

}  // namespace content
