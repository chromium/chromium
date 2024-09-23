// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/webaudio_media_stream_source.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_glitch_info.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

WebAudioMediaStreamSource::WebAudioMediaStreamSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : MediaStreamAudioSource(std::move(task_runner), false /* is_remote */),
      is_registered_consumer_(false),
      fifo_(ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &WebAudioMediaStreamSource::DeliverRebufferedAudio,
          WTF::CrossThreadUnretained(this)))) {
  DVLOG(1) << "WebAudioMediaStreamSource::WebAudioMediaStreamSource()";
}

WebAudioMediaStreamSource::~WebAudioMediaStreamSource() {
  DVLOG(1) << "WebAudioMediaStreamSource::~WebAudioMediaStreamSource()";
  EnsureSourceIsStopped();
}

void WebAudioMediaStreamSource::SetFormat(int number_of_channels,
                                          float sample_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "WebAudio media stream source changed format to: channels="
          << number_of_channels << ", sample_rate=" << sample_rate;

  // If the channel count is greater than 8, use discrete layout. However,
  // anything beyond 8 is ignored by some audio tracks/sinks.
  media::ChannelLayout channel_layout =
      number_of_channels > 8 ? media::CHANNEL_LAYOUT_DISCRETE
                             : media::GuessChannelLayout(number_of_channels);

  // Set the format used by this WebAudioMediaStreamSource. We are using 10ms
  // data as a buffer size since that is the native buffer size of WebRtc packet
  // running on.
  fifo_.Reset(sample_rate / 100);
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                {channel_layout, number_of_channels},
                                sample_rate, fifo_.frames_per_buffer());
  MediaStreamAudioSource::SetFormat(params);

  if (!wrapper_bus_ || wrapper_bus_->channels() != params.channels())
    wrapper_bus_ = media::AudioBus::CreateWrapper(params.channels());
}

bool WebAudioMediaStreamSource::EnsureSourceIsStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_registered_consumer_)
    return true;
  if (!media_stream_source_ || !media_stream_source_->RequiresAudioConsumer())
    return false;
  VLOG(1) << "Starting WebAudio media stream source.";
  media_stream_source_->SetAudioConsumer(this);
  is_registered_consumer_ = true;
  return true;
}

void WebAudioMediaStreamSource::EnsureSourceIsStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_registered_consumer_)
    return;
  is_registered_consumer_ = false;
  DCHECK(media_stream_source_);
  media_stream_source_->RemoveAudioConsumer();
  media_stream_source_ = nullptr;
  VLOG(1) << "Stopped WebAudio media stream source. Final audio parameters={"
          << GetAudioParameters().AsHumanReadableString() << "}.";
}

void WebAudioMediaStreamSource::ConsumeAudio(
    const Vector<const float*>& audio_data,
    int number_of_frames) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamSource::ConsumeAudio", "frames",
               number_of_frames);

  //  TODO(https://crbug.com/1302080): this should use the actual audio
  // playout stamp instead of Now().
  current_reference_time_ = base::TimeTicks::Now();
  wrapper_bus_->set_frames(number_of_frames);
  DCHECK_EQ(wrapper_bus_->channels(), static_cast<int>(audio_data.size()));
  for (wtf_size_t i = 0; i < audio_data.size(); ++i) {
    wrapper_bus_->SetChannelData(static_cast<int>(i),
                                 const_cast<float*>(audio_data[i]));
  }

  // The following will result in zero, one, or multiple synchronous calls to
  // DeliverRebufferedAudio().
  fifo_.Push(*wrapper_bus_);
}

void WebAudioMediaStreamSource::DeliverRebufferedAudio(
    const media::AudioBus& audio_bus,
    int frame_delay) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamSource::DeliverRebufferedAudio", "frames",
               audio_bus.frames());
  const base::TimeTicks reference_time =
      current_reference_time_ +
      base::Microseconds(
          frame_delay * base::Time::kMicrosecondsPerSecond /
          MediaStreamAudioSource::GetAudioParameters().sample_rate());
  MediaStreamAudioSource::DeliverDataToTracks(audio_bus, reference_time, {});
}

}  // namespace blink
