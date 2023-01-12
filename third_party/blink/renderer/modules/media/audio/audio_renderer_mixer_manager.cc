// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_manager.h"

#include <limits>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

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
                                input_params.channel_layout_config(),
                                output_sample_rate, output_buffer_size);

  // Specify the effects info the passed to the browser side.
  params.set_effects(input_params.effects());

  // Specify the latency info to be passed to the browser side.
  params.set_latency_tag(latency);
  return params;
}

}  // namespace

namespace blink {

AudioRendererMixerManager::AudioRendererMixerManager(
    CreateSinkCB create_sink_cb)
    : create_sink_cb_(std::move(create_sink_cb)) {
  DCHECK(create_sink_cb_);
}

AudioRendererMixerManager::~AudioRendererMixerManager() {
  // References to AudioRendererMixers may be owned by garbage collected
  // objects.  During process shutdown they may be leaked, so, transitively,
  // |mixers_| may leak (i.e., may be non-empty at this time) as well.
}

scoped_refptr<media::AudioRendererMixerInput>
AudioRendererMixerManager::CreateInput(
    const blink::LocalFrameToken& source_frame_token,
    const base::UnguessableToken& session_id,
    const std::string& device_id,
    media::AudioLatency::LatencyType latency) {
  // AudioRendererMixerManager lives on the renderer thread and is destroyed on
  // renderer thread destruction, so it's safe to pass its pointer to a mixer
  // input.
  //
  // TODO(olka, grunell): |session_id| is always empty, delete since
  // NewAudioRenderingMixingStrategy didn't ship, https://crbug.com/870836.
  DCHECK(session_id.is_empty());
  return base::MakeRefCounted<media::AudioRendererMixerInput>(
      this, source_frame_token.value(), device_id, latency);
}

media::AudioRendererMixer* AudioRendererMixerManager::GetMixer(
    const blink::LocalFrameToken& source_frame_token,
    const media::AudioParameters& input_params,
    media::AudioLatency::LatencyType latency,
    const media::OutputDeviceInfo& sink_info,
    scoped_refptr<media::AudioRendererSink> sink) {
  // Ownership of the sink must be given to GetMixer().
  DCHECK(sink->HasOneRef());
  DCHECK_EQ(sink_info.device_status(), media::OUTPUT_DEVICE_STATUS_OK);

  const MixerKey key(source_frame_token, input_params, latency,
                     sink_info.device_id());
  base::AutoLock auto_lock(mixers_lock_);

  auto it = mixers_.find(key);
  if (it != mixers_.end()) {
    auto new_count = ++it->second.ref_count;
    CHECK(new_count != std::numeric_limits<decltype(new_count)>::max());

    DVLOG(1) << "Reusing mixer: " << it->second.mixer;

    // Sink will now be released unused, but still must be stopped.
    //
    // TODO(dalecurtis): Is it worth caching this sink instead for a future
    // GetSink() call? We should experiment with a few top sites. We can't just
    // drop in AudioRendererSinkCache here since it doesn't reuse sinks once
    // they've been vended externally to the class.
    sink->Stop();

    return it->second.mixer;
  }

  const media::AudioParameters& mixer_output_params =
      GetMixerOutputParams(input_params, sink_info.output_params(), latency);
  media::AudioRendererMixer* mixer =
      new media::AudioRendererMixer(mixer_output_params, std::move(sink));
  mixers_[key] = {mixer, 1};
  DVLOG(1) << __func__ << " mixer: " << mixer << " latency: " << latency
           << "\n input: " << input_params.AsHumanReadableString()
           << "\noutput: " << mixer_output_params.AsHumanReadableString();
  return mixer;
}

scoped_refptr<media::AudioRendererSink> AudioRendererMixerManager::GetSink(
    const blink::LocalFrameToken& source_frame_token,
    const std::string& device_id) {
  return create_sink_cb_.Run(
      source_frame_token,
      media::AudioSinkParameters(base::UnguessableToken(), device_id));
}

media::AudioRendererMixer* AudioRendererMixerManager::GetMixer(
    const base::UnguessableToken& source_frame_token,
    const media::AudioParameters& input_params,
    media::AudioLatency::LatencyType latency,
    const media::OutputDeviceInfo& sink_info,
    scoped_refptr<media::AudioRendererSink> sink) {
  // Ownership of the sink must be given to GetMixer().
  DCHECK(sink->HasOneRef());
  // Forward to the strongly typed version. We move the |sink| as GetMixer
  // expects to be the sole owner at this point.
  DCHECK(source_frame_token);
  return GetMixer(blink::LocalFrameToken(source_frame_token), input_params,
                  latency, sink_info, std::move(sink));
}

void AudioRendererMixerManager::ReturnMixer(media::AudioRendererMixer* mixer) {
  base::AutoLock auto_lock(mixers_lock_);
  auto it = base::ranges::find(
      mixers_, mixer,
      [](const std::pair<MixerKey, AudioRendererMixerReference>& val) {
        return val.second.mixer;
      });
  DCHECK(it != mixers_.end());

  // Only remove the mixer if AudioRendererMixerManager is the last owner.
  it->second.ref_count--;
  if (it->second.ref_count == 0) {
    delete it->second.mixer;
    mixers_.erase(it);
  }
}

scoped_refptr<media::AudioRendererSink> AudioRendererMixerManager::GetSink(
    const base::UnguessableToken& source_frame_token,
    const std::string& device_id) {
  // Forward to the strongly typed version.
  DCHECK(source_frame_token);
  return GetSink(blink::LocalFrameToken(source_frame_token), device_id);
}

AudioRendererMixerManager::MixerKey::MixerKey(
    const blink::LocalFrameToken& source_frame_token,
    const media::AudioParameters& params,
    media::AudioLatency::LatencyType latency,
    const std::string& device_id)
    : source_frame_token(source_frame_token),
      params(params),
      latency(latency),
      device_id(device_id) {}

AudioRendererMixerManager::MixerKey::MixerKey(const MixerKey& other) = default;

AudioRendererMixerManager::MixerKey::~MixerKey() = default;

}  // namespace blink
