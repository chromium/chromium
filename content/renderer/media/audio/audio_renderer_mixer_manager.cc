// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/audio_renderer_mixer_manager.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/renderer/media/audio/audio_renderer_sink_cache.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"

namespace {

// Calculate mixer output parameters based on mixer input parameters and
// hardware parameters for audio output.
media::AudioParameters GetMixerOutputParams(
    const media::AudioParameters& input_params,
    const media::AudioParameters& hardware_params,
    media::AudioLatency::LatencyType latency) {
  // For a compressed bitstream, no audio post processing is allowed, hence the
  // output parameters should be the same as input parameters.
  if (input_params.IsBitstreamFormat())
    return input_params;

  int output_sample_rate, preferred_output_buffer_size;
  if (!hardware_params.IsValid() ||
      hardware_params.format() == media::AudioParameters::AUDIO_FAKE) {
    // With fake or invalid hardware params, don't waste cycles on resampling.
    output_sample_rate = input_params.sample_rate();
    preferred_output_buffer_size = 0;  // Let media::AudioLatency() choose.
  } else if (media::AudioLatency::IsResamplingPassthroughSupported(latency)) {
    // Certain platforms don't require us to resample to a single rate for low
    // latency, so again, don't waste cycles on resampling.
    output_sample_rate = input_params.sample_rate();

    // For playback, prefer the input params buffer size unless the hardware
    // needs something even larger (say for Bluetooth devices).
    if (latency == media::AudioLatency::LATENCY_PLAYBACK) {
      preferred_output_buffer_size =
          std::max(input_params.frames_per_buffer(),
                   hardware_params.frames_per_buffer());
    } else {
      preferred_output_buffer_size = hardware_params.frames_per_buffer();
    }
  } else {
    // Otherwise, always resample and rebuffer to the hardware parameters.
    output_sample_rate = hardware_params.sample_rate();
    preferred_output_buffer_size = hardware_params.frames_per_buffer();
  }

  int output_buffer_size = 0;

  // Adjust output buffer size according to the latency requirement.
  switch (latency) {
    case media::AudioLatency::LATENCY_INTERACTIVE:
      output_buffer_size = media::AudioLatency::GetInteractiveBufferSize(
          hardware_params.frames_per_buffer());
      break;
    case media::AudioLatency::LATENCY_RTC:
      output_buffer_size = media::AudioLatency::GetRtcBufferSize(
          output_sample_rate, preferred_output_buffer_size);
      break;
    case media::AudioLatency::LATENCY_PLAYBACK:
      output_buffer_size = media::AudioLatency::GetHighLatencyBufferSize(
          output_sample_rate, preferred_output_buffer_size);
      break;
    case media::AudioLatency::LATENCY_EXACT_MS:
    // TODO(olka): add support when WebAudio requires it.
    default:
      NOTREACHED();
  }

  DCHECK_NE(output_buffer_size, 0);

  media::AudioParameters params(input_params.format(),
                                input_params.channel_layout(),
                                output_sample_rate, output_buffer_size);

  // Use the actual channel count when the channel layout is "DISCRETE".
  if (input_params.channel_layout() == media::CHANNEL_LAYOUT_DISCRETE)
    params.set_channels_for_discrete(input_params.channels());

  // Specify the effects info the passed to the browser side.
  params.set_effects(input_params.effects());

  // Specify the latency info to be passed to the browser side.
  params.set_latency_tag(latency);
  return params;
}

void LogMixerUmaHistogram(media::AudioLatency::LatencyType latency, int value) {
  switch (latency) {
    case media::AudioLatency::LATENCY_EXACT_MS:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Media.Audio.Render.AudioInputsPerMixer.LatencyExact", value, 1, 20,
          21);
      return;
    case media::AudioLatency::LATENCY_INTERACTIVE:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Media.Audio.Render.AudioInputsPerMixer.LatencyInteractive", value, 1,
          20, 21);
      return;
    case media::AudioLatency::LATENCY_RTC:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Media.Audio.Render.AudioInputsPerMixer.LatencyRtc", value, 1, 20,
          21);
      return;
    case media::AudioLatency::LATENCY_PLAYBACK:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Media.Audio.Render.AudioInputsPerMixer.LatencyPlayback", value, 1,
          20, 21);
      return;
    default:
      NOTREACHED();
  }
}

}  // namespace

namespace content {

AudioRendererMixerManager::AudioRendererMixerManager(
    std::unique_ptr<AudioRendererSinkCache> sink_cache)
    : sink_cache_(std::move(sink_cache)) {
  DCHECK(sink_cache_);
}

AudioRendererMixerManager::~AudioRendererMixerManager() {
  // References to AudioRendererMixers may be owned by garbage collected
  // objects.  During process shutdown they may be leaked, so, transitively,
  // |mixers_| may leak (i.e., may be non-empty at this time) as well.
}

// static
std::unique_ptr<AudioRendererMixerManager> AudioRendererMixerManager::Create() {
  return base::WrapUnique(
      new AudioRendererMixerManager(AudioRendererSinkCache::Create()));
}

media::AudioRendererMixerInput* AudioRendererMixerManager::CreateInput(
    int source_render_frame_id,
    int session_id,
    const std::string& device_id,
    media::AudioLatency::LatencyType latency) {
  // AudioRendererMixerManager lives on the renderer thread and is destroyed on
  // renderer thread destruction, so it's safe to pass its pointer to a mixer
  // input.
  return new media::AudioRendererMixerInput(
      this, source_render_frame_id,
      media::AudioDeviceDescription::UseSessionIdToSelectDevice(session_id,
                                                                device_id)
          ? GetOutputDeviceInfo(source_render_frame_id, session_id, device_id)
                .device_id()
          : device_id,
      latency);
}

media::AudioRendererMixer* AudioRendererMixerManager::GetMixer(
    int source_render_frame_id,
    const media::AudioParameters& input_params,
    media::AudioLatency::LatencyType latency,
    const std::string& device_id,
    media::OutputDeviceStatus* device_status) {
  const MixerKey key(source_render_frame_id, input_params, latency, device_id);
  base::AutoLock auto_lock(mixers_lock_);

  // Update latency map when the mixer is requested, i.e. there is an attempt to
  // mix and output audio with a given latency. This is opposite to
  // CreateInput() which creates a sink which is probably never used for output.
  if (!latency_map_[latency]) {
    latency_map_[latency] = 1;
    // Log the updated latency map. This can't be done once in the end of the
    // renderer lifetime, because the destructor is usually not called. So,
    // we'll have a sort of exponential scale here, with a smaller subset
    // logged both on its own and as a part of any larger subset.
    base::UmaHistogramSparse("Media.Audio.Render.AudioMixing.LatencyMap",
                             latency_map_.to_ulong());
  }

  auto it = mixers_.find(key);
  if (it != mixers_.end()) {
    if (device_status)
      *device_status = media::OUTPUT_DEVICE_STATUS_OK;

    it->second.ref_count++;
    DVLOG(1) << "Reusing mixer: " << it->second.mixer;
    return it->second.mixer;
  }

  scoped_refptr<media::AudioRendererSink> sink =
      sink_cache_->GetSink(source_render_frame_id, device_id);

  const media::OutputDeviceInfo& device_info = sink->GetOutputDeviceInfo();
  if (device_status)
    *device_status = device_info.device_status();
  if (device_info.device_status() != media::OUTPUT_DEVICE_STATUS_OK) {
    sink_cache_->ReleaseSink(sink.get());
    sink->Stop();
    return nullptr;
  }

  const media::AudioParameters& mixer_output_params =
      GetMixerOutputParams(input_params, device_info.output_params(), latency);
  media::AudioRendererMixer* mixer = new media::AudioRendererMixer(
      mixer_output_params, sink, base::Bind(LogMixerUmaHistogram, latency));
  AudioRendererMixerReference mixer_reference = {mixer, 1, sink.get()};
  mixers_[key] = mixer_reference;
  DVLOG(1) << __func__ << " mixer: " << mixer << " latency: " << latency
           << "\n input: " << input_params.AsHumanReadableString()
           << "\noutput: " << mixer_output_params.AsHumanReadableString();
  return mixer;
}

void AudioRendererMixerManager::ReturnMixer(media::AudioRendererMixer* mixer) {
  base::AutoLock auto_lock(mixers_lock_);
  auto it = std::find_if(
      mixers_.begin(), mixers_.end(),
      [mixer](const std::pair<MixerKey, AudioRendererMixerReference>& val) {
        return val.second.mixer == mixer;
      });
  DCHECK(it != mixers_.end());

  // Only remove the mixer if AudioRendererMixerManager is the last owner.
  it->second.ref_count--;
  if (it->second.ref_count == 0) {
    // The mixer will be deleted now, so release the sink.
    sink_cache_->ReleaseSink(it->second.sink_ptr);
    delete it->second.mixer;
    mixers_.erase(it);
  }
}

media::OutputDeviceInfo AudioRendererMixerManager::GetOutputDeviceInfo(
    int source_render_frame_id,
    int session_id,
    const std::string& device_id) {
  return sink_cache_->GetSinkInfo(source_render_frame_id, session_id,
                                  device_id);
}

AudioRendererMixerManager::MixerKey::MixerKey(
    int source_render_frame_id,
    const media::AudioParameters& params,
    media::AudioLatency::LatencyType latency,
    const std::string& device_id)
    : source_render_frame_id(source_render_frame_id),
      params(params),
      latency(latency),
      device_id(device_id) {}

AudioRendererMixerManager::MixerKey::MixerKey(const MixerKey& other) = default;

}  // namespace content
