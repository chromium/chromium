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
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_input.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace {

// Calculate mixer output parameters based on mixer input parameters and
// hardware parameters for audio output.
media::AudioParameters GetMixerOutputParams(
    const media::AudioParameters& input_params,
    const media::AudioParameters& hardware_params,
    media::AudioLatency::Type latency) {
  // For a compressed bitstream, no audio post processing is allowed, hence the
  // output parameters should be the same as input parameters.
  if (input_params.IsBitstreamFormat()) {
    return input_params;
  }

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
    if (latency == media::AudioLatency::Type::kPlayback) {
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
    case media::AudioLatency::Type::kInteractive:
      output_buffer_size = media::AudioLatency::GetInteractiveBufferSize(
          hardware_params.frames_per_buffer());
      break;
    case media::AudioLatency::Type::kRtc:
      output_buffer_size = media::AudioLatency::GetRtcBufferSize(
          output_sample_rate, preferred_output_buffer_size);
      break;
    case media::AudioLatency::Type::kPlayback:
      output_buffer_size = media::AudioLatency::GetHighLatencyBufferSize(
          output_sample_rate, preferred_output_buffer_size);
      break;
    case media::AudioLatency::Type::kExactMS:
    // TODO(olka): add support when WebAudio requires it.
    default:
      NOTREACHED_IN_MIGRATION();
  }

  DCHECK_NE(output_buffer_size, 0);

  media::AudioParameters params(input_params.format(),
                                input_params.channel_layout_config(),
                                output_sample_rate, output_buffer_size);

  // Specify the effects info the passed to the browser side.
  params.set_effects(input_params.effects());

  // Specify the latency info to be passed to the browser side.
  params.set_latency_tag(latency);

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(media::kAudioOffload)) {
    if (params.latency_tag() == media::AudioLatency::Type::kPlayback) {
      media::AudioParameters::HardwareCapabilities hardware_caps(0, 0, 0, true);
      params.set_hardware_capabilities(hardware_caps);
    }
  }
#endif
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
  // `mixers_` may leak (i.e., may be non-empty at this time) as well.
}

scoped_refptr<AudioRendererMixerInput> AudioRendererMixerManager::CreateInput(
    const LocalFrameToken& source_frame_token,
    const FrameToken& main_frame_token,
    const base::UnguessableToken& session_id,
    std::string_view device_id,
    media::AudioLatency::Type latency) {
  // AudioRendererMixerManager lives on the renderer thread and is destroyed on
  // renderer thread destruction, so it's safe to pass its pointer to a mixer
  // input.
  //
  // TODO(crbug.com/41405939): `session_id` is always empty, delete since
  // NewAudioRenderingMixingStrategy didn't ship.
  DCHECK(session_id.is_empty());
  return base::MakeRefCounted<AudioRendererMixerInput>(
      this, source_frame_token, main_frame_token, device_id, latency);
}

AudioRendererMixer* AudioRendererMixerManager::GetMixer(
    const FrameToken& main_frame_token,
    const media::AudioParameters& input_params,
    media::AudioLatency::Type latency,
    const media::OutputDeviceInfo& sink_info,
    scoped_refptr<media::AudioRendererSink> sink) {
  // Ownership of the sink must be given to GetMixer().
  DCHECK(sink->HasOneRef());

  // It's important that `sink` has already been authorized to ensure we don't
  // allow sharing between RenderFrames not authorized for sending audio to a
  // given device.
  CHECK_EQ(sink_info.device_status(), media::OUTPUT_DEVICE_STATUS_OK);

  const MixerKey key(main_frame_token, input_params, latency,
                     sink_info.device_id());
  base::AutoLock auto_lock(mixers_lock_);

  auto it = mixers_.find(key);
  if (it != mixers_.end() && !it->second.mixer->HasSinkError()) {
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

    return it->second.mixer.get();
  } else if (it != mixers_.end() && it->second.mixer->HasSinkError()) {
    DVLOG(1) << "Not reusing mixer with errors: " << it->second.mixer;

    // Move bad mixers out of the reuse map.
    dead_mixers_.emplace_back(std::move(it->second.mixer),
                              it->second.ref_count);
    mixers_.erase(it);
  }

  const auto mixer_output_params =
      GetMixerOutputParams(input_params, sink_info.output_params(), latency);
  auto mixer = std::make_unique<AudioRendererMixer>(mixer_output_params,
                                                    std::move(sink));
  auto* mixer_ref = mixer.get();
  mixers_[key] = {std::move(mixer), 1};
  DVLOG(1) << __func__ << " mixer: " << mixer
           << " latency: " << base::to_underlying(latency)
           << "\n input: " << input_params.AsHumanReadableString()
           << "\noutput: " << mixer_output_params.AsHumanReadableString();
  return mixer_ref;
}

void AudioRendererMixerManager::ReturnMixer(AudioRendererMixer* mixer) {
  base::AutoLock auto_lock(mixers_lock_);
  auto it = base::ranges::find(
      mixers_, mixer,
      [](const std::pair<MixerKey, AudioRendererMixerReference>& val) {
        return val.second.mixer.get();
      });

  // If a mixer isn't in the normal map, check the map for mixers w/ errors.
  auto dead_it = dead_mixers_.end();
  if (it == mixers_.end()) {
    dead_it = base::ranges::find(
        dead_mixers_, mixer,
        [](const AudioRendererMixerReference& val) { return val.mixer.get(); });
    CHECK(dead_it != dead_mixers_.end(), base::NotFatalUntil::M130);
  }

  auto& mixer_ref = it == mixers_.end() ? *dead_it : it->second;

  // Only remove the mixer if AudioRendererMixerManager is the last owner.
  mixer_ref.ref_count--;
  if (mixer_ref.ref_count == 0) {
    if (dead_it != dead_mixers_.end()) {
      dead_mixers_.erase(dead_it);
    } else {
      mixers_.erase(it);
    }
  } else if (dead_it == dead_mixers_.end() && mixer_ref.mixer->HasSinkError()) {
    // Move bad mixers out of the reuse map.
    dead_mixers_.emplace_back(std::move(mixer_ref.mixer), mixer_ref.ref_count);
    mixers_.erase(it);
  }
}

scoped_refptr<media::AudioRendererSink> AudioRendererMixerManager::GetSink(
    const LocalFrameToken& source_frame_token,
    std::string_view device_id) {
  return create_sink_cb_.Run(
      source_frame_token, media::AudioSinkParameters(base::UnguessableToken(),
                                                     std::string(device_id)));
}

AudioRendererMixerManager::MixerKey::MixerKey(
    const FrameToken& main_frame_token,
    const media::AudioParameters& params,
    media::AudioLatency::Type latency,
    std::string_view device_id)
    : main_frame_token(main_frame_token),
      params(params),
      latency(latency),
      device_id(device_id) {}

AudioRendererMixerManager::MixerKey::MixerKey(const MixerKey& other) = default;

AudioRendererMixerManager::MixerKey::~MixerKey() = default;

}  // namespace blink
