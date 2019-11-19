// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/webaudio_media_stream_source.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"

namespace blink {

WebAudioMediaStreamSource::WebAudioMediaStreamSource(
    WebMediaStreamSource* blink_source,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : MediaStreamAudioSource(std::move(task_runner), false /* is_remote */),
      is_registered_consumer_(false),
      fifo_(base::Bind(&WebAudioMediaStreamSource::DeliverRebufferedAudio,
                       base::Unretained(this))),
      blink_source_(*blink_source) {
  DVLOG(1) << "WebAudioMediaStreamSource::WebAudioMediaStreamSource()";
}

WebAudioMediaStreamSource::~WebAudioMediaStreamSource() {
  DVLOG(1) << "WebAudioMediaStreamSource::~WebAudioMediaStreamSource()";
  EnsureSourceIsStopped();
}

void WebAudioMediaStreamSource::SetFormat(size_t number_of_channels,
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
  //
  // TODO(miu): Re-evaluate whether this is needed. For now (this refactoring),
  // I did not want to change behavior. https://crbug.com/577874
  fifo_.Reset(sample_rate / 100);
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                channel_layout, sample_rate,
                                fifo_.frames_per_buffer());
  // Take care of the discrete channel layout case.
  params.set_channels_for_discrete(number_of_channels);
  MediaStreamAudioSource::SetFormat(params);

  if (!wrapper_bus_ || wrapper_bus_->channels() != params.channels())
    wrapper_bus_ = media::AudioBus::CreateWrapper(params.channels());
}

bool WebAudioMediaStreamSource::EnsureSourceIsStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_registered_consumer_)
    return true;
  if (blink_source_.IsNull() || !blink_source_.RequiresAudioConsumer())
    return false;
  VLOG(1) << "Starting WebAudio media stream source.";
  blink_source_.AddAudioConsumer(this);
  is_registered_consumer_ = true;
  return true;
}

void WebAudioMediaStreamSource::EnsureSourceIsStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_registered_consumer_)
    return;
  is_registered_consumer_ = false;
  DCHECK(!blink_source_.IsNull());
  blink_source_.RemoveAudioConsumer(this);
  blink_source_.Reset();
  VLOG(1) << "Stopped WebAudio media stream source. Final audio parameters={"
          << GetAudioParameters().AsHumanReadableString() << "}.";
}

void WebAudioMediaStreamSource::ConsumeAudio(
    const WebVector<const float*>& audio_data,
    size_t number_of_frames) {
  // TODO(miu): Plumbing is needed to determine the actual capture timestamp
  // of the audio, instead of just snapshotting base::TimeTicks::Now(), for
  // proper audio/video sync.  https://crbug.com/335335
  current_reference_time_ = base::TimeTicks::Now();

  wrapper_bus_->set_frames(number_of_frames);
  DCHECK_EQ(wrapper_bus_->channels(), static_cast<int>(audio_data.size()));
  for (size_t i = 0; i < audio_data.size(); ++i)
    wrapper_bus_->SetChannelData(i, const_cast<float*>(audio_data[i]));

  // The following will result in zero, one, or multiple synchronous calls to
  // DeliverRebufferedAudio().
  fifo_.Push(*wrapper_bus_);
}

void WebAudioMediaStreamSource::DeliverRebufferedAudio(
    const media::AudioBus& audio_bus,
    int frame_delay) {
  const base::TimeTicks reference_time =
      current_reference_time_ +
      base::TimeDelta::FromMicroseconds(
          frame_delay * base::Time::kMicrosecondsPerSecond /
          MediaStreamAudioSource::GetAudioParameters().sample_rate());
  MediaStreamAudioSource::DeliverDataToTracks(audio_bus, reference_time);
}

}  // namespace blink
