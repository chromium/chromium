// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/aec_dump_agent_impl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {
void WebRtcLogStringPiece(std::string_view message) {
  WebRtcLogMessage(std::string{message});
}
}  // namespace

// Subscribes a sink to the playout data source for the duration of the
// PlayoutListener lifetime.
class MediaStreamAudioProcessor::PlayoutListener {
 public:
  PlayoutListener(scoped_refptr<WebRtcAudioDeviceImpl> playout_data_source,
                  WebRtcPlayoutDataSource::Sink* sink)
      : playout_data_source_(std::move(playout_data_source)), sink_(sink) {
    DCHECK(playout_data_source_);
    DCHECK(sink_);
    playout_data_source_->AddPlayoutSink(sink_);
  }

  ~PlayoutListener() { playout_data_source_->RemovePlayoutSink(sink_); }

 private:
  // TODO(crbug.com/704136): Replace with Member at some point.
  scoped_refptr<WebRtcAudioDeviceImpl> const playout_data_source_;
  const raw_ptr<WebRtcPlayoutDataSource::Sink> sink_;
};

MediaStreamAudioProcessor::MediaStreamAudioProcessor(
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    const media::AudioProcessingSettings& settings,
    const media::AudioParameters& capture_data_source_params,
    scoped_refptr<WebRtcAudioDeviceImpl> playout_data_source)
    : audio_processor_(media::AudioProcessor::Create(
          std::move(deliver_processed_audio_callback),
          /*log_callback=*/
          WTF::BindRepeating(&WebRtcLogStringPiece),
          settings,
          capture_data_source_params,
          media::AudioProcessor::GetDefaultOutputFormat(
              capture_data_source_params,
              settings))),
      main_thread_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      aec_dump_agent_impl_(AecDumpAgentImpl::Create(this)),
      stopped_(false) {
  DCHECK(main_thread_runner_);
  // Register as a listener for the playout reference signal. Used for e.g. echo
  // cancellation.
  if (audio_processor_->needs_playout_reference() && playout_data_source) {
    playout_listener_ =
        std::make_unique<PlayoutListener>(std::move(playout_data_source), this);
  }
  DETACH_FROM_THREAD(capture_thread_checker_);
  DETACH_FROM_THREAD(render_thread_checker_);
}

MediaStreamAudioProcessor::~MediaStreamAudioProcessor() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  Stop();
}

void MediaStreamAudioProcessor::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    int num_preferred_channels,
    double volume,
    bool key_pressed) {
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);
  audio_processor_->ProcessCapturedAudio(audio_source, audio_capture_time,
                                         num_preferred_channels, volume,
                                         key_pressed);
}

void MediaStreamAudioProcessor::Stop() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  if (stopped_)
    return;
  stopped_ = true;

  aec_dump_agent_impl_.reset();
  audio_processor_->OnStopDump();
  playout_listener_.reset();
}

const media::AudioParameters&
MediaStreamAudioProcessor::GetInputFormatForTesting() const {
  return audio_processor_->input_format();
}

void MediaStreamAudioProcessor::OnStartDump(base::File dump_file) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  audio_processor_->OnStartDump(std::move(dump_file));
}

void MediaStreamAudioProcessor::OnStopDump() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  audio_processor_->OnStopDump();
}

// static
// TODO(https://crbug.com/1269364): This logic should be moved to
// ProcessedLocalAudioSource and verified/fixed; The decision should be
// "hardware effects are required or software audio mofidications are needed
// (AudioProcessingSettings.NeedAudioModification())".
bool MediaStreamAudioProcessor::WouldModifyAudio(
    const AudioProcessingProperties& properties) {
  if (properties
          .ToAudioProcessingSettings(
              /*multi_channel_capture_processing - does not matter here*/ false)
          .NeedAudioModification()) {
    return true;
  }

#if !BUILDFLAG(IS_IOS)
  if (properties.goog_auto_gain_control) {
    return true;
  }
#endif

  if (properties.goog_noise_suppression) {
    return true;
  }

  return false;
}

void MediaStreamAudioProcessor::OnPlayoutData(media::AudioBus* audio_bus,
                                              int sample_rate,
                                              base::TimeDelta audio_delay) {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  DCHECK(audio_bus);
  audio_processor_->OnPlayoutData(*audio_bus, sample_rate, audio_delay);
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
  stats.apm_statistics = audio_processor_->GetStats();
  return stats;
}

}  // namespace blink
