// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_processor_handler.h"

#include "base/cxx17_backports.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

namespace audio {

AudioProcessorHandler::AudioProcessorHandler(
    const media::AudioProcessingSettings& settings,
    const media::AudioParameters& input_format,
    const media::AudioParameters& output_format,
    LogCallback log_callback,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    mojo::PendingReceiver<media::mojom::AudioProcessorControls>
        controls_receiver,
    media::AecdumpRecordingManager* aecdump_recording_manager)
    : audio_processor_(media::AudioProcessor::Create(
          std::move(deliver_processed_audio_callback),
          std::move(log_callback),
          settings,
          input_format,
          output_format)),
      receiver_(this, std::move(controls_receiver)),
      aecdump_recording_manager_(aecdump_recording_manager) {
  DCHECK(settings.NeedAudioModification());
  if (aecdump_recording_manager_) {
    aecdump_recording_manager->RegisterAecdumpSource(this);
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
    bool key_pressed) {
  const int num_preferred_channels =
      num_preferred_channels_.load(std::memory_order_acquire);
  audio_processor_->ProcessCapturedAudio(audio_source, audio_capture_time,
                                         num_preferred_channels, volume,
                                         key_pressed);
}

void AudioProcessorHandler::OnPlayoutData(const media::AudioBus& audio_bus,
                                          int sample_rate,
                                          base::TimeDelta delay) {
  TRACE_EVENT2("audio", "AudioProcessorHandler::OnPlayoutData", " this ",
               static_cast<void*>(this), "delay", delay.InMillisecondsF());
  audio_processor_->OnPlayoutData(audio_bus, sample_rate, delay);
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
  num_preferred_channels = base::clamp(
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
}  // namespace audio
