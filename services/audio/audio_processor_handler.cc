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
#include "services/audio/ml_model_manager.h"

namespace audio {

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
    raw_ptr<MlModelManager> ml_model_manager)
    : residual_echo_estimation_model_handle_(
          ml_model_manager ? ml_model_manager->GetResidualEchoEstimationModel()
                           : nullptr),
      audio_processor_(media::AudioProcessor::Create(
          // Unretained is safe because this class owns audio_processor_, so it
          // will be destroyed first.
          base::BindRepeating(&AudioProcessorHandler::DeliverProcessedAudio,
                              base::Unretained(this)),
          std::move(log_callback),
          settings,
          input_format,
          output_format,
          residual_echo_estimation_model_handle_
              ? residual_echo_estimation_model_handle_->Get()
              : nullptr)),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      reference_stream_error_callback_(
          std::move(reference_stream_error_callback)),
      receiver_(this, std::move(controls_receiver)),
      aecdump_recording_manager_(aecdump_recording_manager) {
  DCHECK(settings.NeedWebrtcAudioProcessing());
  if (aecdump_recording_manager_) {
    aecdump_recording_manager->RegisterAecdumpSource(this);
  }
  if (media::IsAudioProcessMlModelUsageEnabled() &&
      settings.echo_cancellation) {
    // Only log model availability when model management is enabled and echo
    // cancellation is requested, in order to avoid diluting the metric.
    // We log it here, in the audio service, because lower layers are also
    // used from render processes where this feature is not available.
    bool is_model_available = residual_echo_estimation_model_handle_ != nullptr;
    base::UmaHistogramBoolean(
        "Media.Audio.Capture.NeuralResidualEchoEstimationModelAvailable",
        is_model_available);
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

void AudioProcessorHandler::ProcessCapturedAudio(
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

void AudioProcessorHandler::StartAecdump(base::File aecdump_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  audio_processor_->OnStartDump(std::move(aecdump_file));
}

void AudioProcessorHandler::StopAecdump() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  audio_processor_->OnStopDump();
}

void AudioProcessorHandler::DeliverProcessedAudio(
    const media::AudioBus& audio_bus,
    base::TimeTicks audio_capture_time,
    std::optional<double> new_volume) {
  deliver_processed_audio_callback_.Run(audio_bus, audio_capture_time,
                                        new_volume,
                                        glitch_info_accumulator_.GetAndReset());
}
}  // namespace audio
