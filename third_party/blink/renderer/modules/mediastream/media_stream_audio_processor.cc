// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"

#include <memory>

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/aec_dump_agent_impl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

MediaStreamAudioProcessor::MediaStreamAudioProcessor(
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    const AudioProcessingProperties& properties,
    bool use_capture_multi_channel_processing,
    scoped_refptr<WebRtcAudioDeviceImpl> playout_data_source)
    : audio_processor_(std::move(deliver_processed_audio_callback),
                       /*log_callback=*/
                       ConvertToBaseRepeatingCallback(
                           CrossThreadBindRepeating(&WebRtcLogMessage)),
                       properties.ToAudioProcessingSettings(
                           use_capture_multi_channel_processing)),
      playout_data_source_(std::move(playout_data_source)),
      main_thread_runner_(base::ThreadTaskRunnerHandle::Get()),
      aec_dump_agent_impl_(AecDumpAgentImpl::Create(this)),
      stopped_(false) {
  DCHECK(main_thread_runner_);
  // Register as a listener for the playout reference signal. Used for echo
  // cancellation and gain control.
  if (audio_processor_.RequiresPlayoutReference() && playout_data_source_) {
    playout_data_source_->AddPlayoutSink(this);
  }
  DETACH_FROM_THREAD(capture_thread_checker_);
  DETACH_FROM_THREAD(render_thread_checker_);
}

MediaStreamAudioProcessor::~MediaStreamAudioProcessor() {
  // TODO(miu): This class is ref-counted, shared among threads, and then
  // requires itself to be destroyed on the main thread only?!?!? Fix this, and
  // then remove the hack in WebRtcAudioSink::Adapter.
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  Stop();
}

void MediaStreamAudioProcessor::OnCaptureFormatChanged(
    const media::AudioParameters& input_format) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  audio_processor_.OnCaptureFormatChanged(input_format);

  // Reset the |capture_thread_checker_| since the capture data will come from
  // a new capture thread.
  DETACH_FROM_THREAD(capture_thread_checker_);
}

void MediaStreamAudioProcessor::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    int num_preferred_channels,
    double volume,
    bool key_pressed) {
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);
  audio_processor_.ProcessCapturedAudio(audio_source, audio_capture_time,
                                        num_preferred_channels, volume,
                                        key_pressed);
}

void MediaStreamAudioProcessor::Stop() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  if (stopped_)
    return;
  stopped_ = true;

  aec_dump_agent_impl_.reset();
  audio_processor_.OnStopDump();
  if (audio_processor_.RequiresPlayoutReference() && playout_data_source_) {
    playout_data_source_->RemovePlayoutSink(this);
    playout_data_source_ = nullptr;
  }
}

const media::AudioParameters&
MediaStreamAudioProcessor::GetInputFormatForTesting() const {
  return audio_processor_.GetInputFormatForTesting();
}

const media::AudioParameters& MediaStreamAudioProcessor::OutputFormat() const {
  return audio_processor_.OutputFormat();
}

void MediaStreamAudioProcessor::SetOutputWillBeMuted(bool muted) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DCHECK(base::FeatureList::IsEnabled(
      features::kMinimizeAudioProcessingForUnusedOutput));
  audio_processor_.SetOutputWillBeMuted(muted);
}

void MediaStreamAudioProcessor::OnStartDump(base::File dump_file) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  audio_processor_.OnStartDump(std::move(dump_file));
}

void MediaStreamAudioProcessor::OnStopDump() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  audio_processor_.OnStopDump();
}

// static
bool MediaStreamAudioProcessor::WouldModifyAudio(
    const AudioProcessingProperties& properties) {
  // Note: This method should be kept in-sync with any changes to the logic in
  // AudioProcessor::InitializeAudioProcessingModule().
  // TODO(https://crbug.com/1269364): Share this logic with
  // AudioProcessor::InitializeAudioProcessingModule().

  if (properties.goog_audio_mirroring)
    return true;

#if !defined(OS_IOS)
  if (properties.EchoCancellationIsWebRtcProvided() ||
      properties.goog_auto_gain_control) {
    return true;
  }
#endif

#if !defined(OS_IOS) && !defined(OS_ANDROID)
  if (properties.goog_experimental_echo_cancellation) {
    return true;
  }
#endif

  if (properties.goog_noise_suppression ||
      properties.goog_experimental_noise_suppression ||
      properties.goog_highpass_filter) {
    return true;
  }

  return false;
}

void MediaStreamAudioProcessor::OnPlayoutData(media::AudioBus* audio_bus,
                                              int sample_rate,
                                              base::TimeDelta audio_delay) {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  audio_processor_.OnPlayoutData(audio_bus, sample_rate, audio_delay);
}

void MediaStreamAudioProcessor::OnPlayoutDataSourceChanged() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DETACH_FROM_THREAD(render_thread_checker_);
}

void MediaStreamAudioProcessor::OnRenderThreadChanged() {
  DETACH_FROM_THREAD(render_thread_checker_);
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
}

webrtc::AudioProcessorInterface::AudioProcessorStatistics
MediaStreamAudioProcessor::GetStats(bool has_remote_tracks) {
  AudioProcessorStatistics stats;
  stats.apm_statistics = audio_processor_.GetStats();
  return stats;
}

}  // namespace blink
