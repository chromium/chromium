// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_processor_handler.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "media/webrtc/ml_model_handle.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "services/audio/ml_model_manager.h"
#include "services/audio/processing_audio_fifo.h"
#include "services/audio/voice_isolation_handler.h"

namespace audio {
namespace {

scoped_refptr<media::MlModelHandle> GetAndLogResidualEchoEstimationModel(
    MlModelManager* ml_model_manager,
    bool echo_cancellation) {
  scoped_refptr<media::MlModelHandle> model;
  if (ml_model_manager) {
    model =
        ml_model_manager->GetModel(mojom::MlModelType::kResidualEchoEstimation);
  }

  // Only log model availability when ML echo estimation is enabled and echo
  // cancellation is requested, in order to avoid diluting the metric.
  // We log it here, in the audio service, because lower layers are also used
  // from render processes where this feature is not available.
  if (media::IsAudioProcessMlModelUsageEnabled() &&
      base::FeatureList::IsEnabled(
          media::kWebRtcAudioNeuralResidualEchoEstimation) &&
      echo_cancellation) {
    base::UmaHistogramBoolean(
        "Media.Audio.Capture.NeuralResidualEchoEstimationModelAvailable",
        model != nullptr);
  }
  return model;
}

}  // namespace

AudioProcessorHandler::AudioProcessorHandler(
    const media::AudioProcessingSettings& settings,
    const media::AudioParameters& input_format,
    const media::AudioParameters& output_format,
    LogCallback log_callback,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    ReferenceStreamErrorCallback reference_stream_error_callback,
    mojo::PendingReceiver<media::mojom::AudioProcessorControls>
        controls_receiver,
    media::AecdumpRecordingManager* aecdump_recording_manager,
    raw_ptr<MlModelManager> ml_model_manager,
    std::unique_ptr<VoiceIsolationHandler> voice_isolation_handler)
    : voice_isolation_handler_(std::move(voice_isolation_handler)),
      audio_processor_(media::AudioProcessor::Create(
          // Unretained is safe because this class owns audio_processor_, so it
          // will be destroyed first.
          base::BindRepeating(&AudioProcessorHandler::OnAudioProcessorOutput,
                              base::Unretained(this)),
          log_callback,
          settings,
          input_format,
          output_format,
          GetAndLogResidualEchoEstimationModel(ml_model_manager,
                                               settings.echo_cancellation))),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      reference_stream_error_callback_(
          std::move(reference_stream_error_callback)),
      receiver_(this, std::move(controls_receiver)),
      aecdump_recording_manager_(aecdump_recording_manager) {
  DCHECK(settings.NeedWebrtcAudioProcessing());
  // One and only one is defined.
  CHECK(deliver_processed_audio_callback_.is_null() !=
        (voice_isolation_handler_ == nullptr));
  // Voice isolation handler must be provided if and only if voice isolation is
  // enabled in settings.
  CHECK_EQ(voice_isolation_handler_ != nullptr, settings.voice_isolation);
  if (aecdump_recording_manager_) {
    aecdump_recording_manager->RegisterAecdumpSource(this);
  }

  // We need to offload work to another thread for heavy processing, ex: echo
  // cancellation.
  if (needs_playout_reference()) {
    processing_fifo_ = std::make_unique<ProcessingAudioFifo>(
        input_format, kProcessingFifoSize,
        base::BindRepeating(
            &AudioProcessorHandler::ProcessCapturedAudioInternal,
            base::Unretained(this)),
        std::move(log_callback));
  }
}

AudioProcessorHandler::~AudioProcessorHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (aecdump_recording_manager_) {
    // If an aecdump is currently ongoing, this will trigger a StopAecdump()
    // call.
    aecdump_recording_manager_->DeregisterAecdumpSource(this);
  }
}

void AudioProcessorHandler::StartProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  // This is safe because the caller is required to call StartProcessing()
  // before the capture stream is started, ensuring no concurrent calls to
  // ProcessCapturedAudio() can occur.
  if (processing_fifo_) {
    processing_fifo_->Start();
  }
}

void AudioProcessorHandler::StopProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  // This is safe because the caller is required to synchronously stop the
  // capture stream before calling StopProcessing(), guaranteeing that no
  // concurrent calls to ProcessCapturedAudio() can occur.
  processing_fifo_.reset();
}

void AudioProcessorHandler::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    double volume,
    const media::AudioGlitchInfo& audio_glitch_info) {
  if (processing_fifo_) {
    processing_fifo_->PushData(&audio_source, audio_capture_time, volume,
                               audio_glitch_info);
  } else {
    ProcessCapturedAudioInternal(audio_source, audio_capture_time, volume,
                                 audio_glitch_info);
  }
}

void AudioProcessorHandler::ProcessCapturedAudioInternal(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    double volume,
    const media::AudioGlitchInfo& audio_glitch_info) {
  glitch_info_accumulator_.Add(audio_glitch_info);
  const int num_preferred_channels =
      num_preferred_channels_.load(std::memory_order_acquire);
  audio_processor_->ProcessCapturedAudio(audio_source, audio_capture_time,
                                         num_preferred_channels, volume);
}

void AudioProcessorHandler::OnPlayoutData(const media::AudioBus& audio_bus,
                                          int sample_rate,
                                          base::TimeDelta delay) {
  TRACE_EVENT2("audio", "AudioProcessorHandler::OnPlayoutData", " this ",
               static_cast<void*>(this), "delay", delay.InMillisecondsF());
  audio_processor_->OnPlayoutData(audio_bus, sample_rate, delay);
}

void AudioProcessorHandler::OnReferenceStreamError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  reference_stream_error_callback_.Run();
}

void AudioProcessorHandler::GetStats(GetStatsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  media::AudioProcessingStats stats;
  const webrtc::AudioProcessingStats processor_stats =
      audio_processor_->GetStats();
  stats.echo_return_loss = processor_stats.echo_return_loss;
  stats.echo_return_loss_enhancement =
      processor_stats.echo_return_loss_enhancement;
  std::move(callback).Run(stats);
}

void AudioProcessorHandler::SetPreferredNumCaptureChannels(
    int32_t num_preferred_channels) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  num_preferred_channels = std::clamp(
      num_preferred_channels, 1, audio_processor_->output_format().channels());
  num_preferred_channels_.store(num_preferred_channels,
                                std::memory_order_release);
}

void AudioProcessorHandler::SetVoiceIsolation(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!voice_isolation_handler_) {
    // Voice isolation cannot be enabled if it is not available (i.e. was not
    // requested initially).
    if (enabled) {
      receiver_.ReportBadMessage("Voice isolation cannot be enabled.");
    }
    return;
  }
  voice_isolation_handler_->SetVoiceIsolation(enabled);
}

void AudioProcessorHandler::StartAecdump(base::File aecdump_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  audio_processor_->OnStartDump(std::move(aecdump_file));
}

void AudioProcessorHandler::StopAecdump() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  audio_processor_->OnStopDump();
}

void AudioProcessorHandler::OnAudioProcessorOutput(
    const media::AudioBus& audio_bus,
    base::TimeTicks audio_capture_time,
    std::optional<double> new_volume) {
  TRACE_EVENT("audio", "AudioProcessorHandler::OnAudioProcessorOutput");
  // Retrieve and reset the accumulated glitch info to ensure it is attached
  // to the processed frame.
  const media::AudioGlitchInfo glitch_info =
      glitch_info_accumulator_.GetAndReset();

  if (voice_isolation_handler_) {
    // Route the processed audio and its metadata through voice isolation.
    voice_isolation_handler_->ProcessCapturedAudio(
        audio_bus, audio_capture_time, new_volume, glitch_info);
  } else {
    // Deliver directly to the final destination callback.
    deliver_processed_audio_callback_.Run(audio_bus, audio_capture_time,
                                          new_volume, glitch_info);
  }
}
}  // namespace audio
