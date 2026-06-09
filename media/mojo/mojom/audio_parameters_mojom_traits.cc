// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_parameters_mojom_traits.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_point.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

namespace mojo {

// static
media::mojom::AudioParametersFormat EnumTraits<
    media::mojom::AudioParametersFormat,
    media::AudioParameters::Format>::ToMojom(media::AudioParameters::Format
                                                 input) {
  switch (input) {
    case media::AudioParameters::AUDIO_FAKE:
      return media::mojom::AudioParametersFormat::kFake;
    case media::AudioParameters::AUDIO_PCM_LINEAR:
      return media::mojom::AudioParametersFormat::kPcmLinear;
    case media::AudioParameters::AUDIO_PCM_LOW_LATENCY:
      return media::mojom::AudioParametersFormat::kPcmLowLatency;
    case media::AudioParameters::AUDIO_BITSTREAM_AC3:
      return media::mojom::AudioParametersFormat::kBitstreamAc3;
    case media::AudioParameters::AUDIO_BITSTREAM_EAC3:
      return media::mojom::AudioParametersFormat::kBitstreamEac3;
    case media::AudioParameters::AUDIO_BITSTREAM_DTS:
      return media::mojom::AudioParametersFormat::kBitstreamDts;
    case media::AudioParameters::AUDIO_BITSTREAM_DTS_HD:
      return media::mojom::AudioParametersFormat::kBitstreamDtsHd;
    case media::AudioParameters::AUDIO_BITSTREAM_DTSX_P2:
      return media::mojom::AudioParametersFormat::kBitstreamDtsxP2;
    case media::AudioParameters::AUDIO_BITSTREAM_IEC61937:
      return media::mojom::AudioParametersFormat::kBitstreamIec61937;
    case media::AudioParameters::AUDIO_BITSTREAM_DTS_HD_MA:
      return media::mojom::AudioParametersFormat::kBitstreamDtsHdMa;
  }
  NOTREACHED();
}

// static
media::AudioParameters::Format EnumTraits<media::mojom::AudioParametersFormat,
                                          media::AudioParameters::Format>::
    FromMojom(media::mojom::AudioParametersFormat input) {
  switch (input) {
    case media::mojom::AudioParametersFormat::kFake:
      return media::AudioParameters::AUDIO_FAKE;
    case media::mojom::AudioParametersFormat::kPcmLinear:
      return media::AudioParameters::AUDIO_PCM_LINEAR;
    case media::mojom::AudioParametersFormat::kPcmLowLatency:
      return media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
    case media::mojom::AudioParametersFormat::kBitstreamAc3:
      return media::AudioParameters::AUDIO_BITSTREAM_AC3;
    case media::mojom::AudioParametersFormat::kBitstreamEac3:
      return media::AudioParameters::AUDIO_BITSTREAM_EAC3;
    case media::mojom::AudioParametersFormat::kBitstreamDts:
      return media::AudioParameters::AUDIO_BITSTREAM_DTS;
    case media::mojom::AudioParametersFormat::kBitstreamDtsHd:
      return media::AudioParameters::AUDIO_BITSTREAM_DTS_HD;
    case media::mojom::AudioParametersFormat::kBitstreamDtsxP2:
      return media::AudioParameters::AUDIO_BITSTREAM_DTSX_P2;
    case media::mojom::AudioParametersFormat::kBitstreamIec61937:
      return media::AudioParameters::AUDIO_BITSTREAM_IEC61937;
    case media::mojom::AudioParametersFormat::kBitstreamDtsHdMa:
      return media::AudioParameters::AUDIO_BITSTREAM_DTS_HD_MA;
  }
  NOTREACHED();
}

// static
media::mojom::AudioLatencyType
EnumTraits<media::mojom::AudioLatencyType, media::AudioLatency::Type>::ToMojom(
    media::AudioLatency::Type input) {
  switch (input) {
    case media::AudioLatency::Type::kExactMS:
      return media::mojom::AudioLatencyType::kExactMs;
    case media::AudioLatency::Type::kInteractive:
      return media::mojom::AudioLatencyType::kInteractive;
    case media::AudioLatency::Type::kRtc:
      return media::mojom::AudioLatencyType::kRtc;
    case media::AudioLatency::Type::kPlayback:
      return media::mojom::AudioLatencyType::kPlayback;
    case media::AudioLatency::Type::kUnknown:
      return media::mojom::AudioLatencyType::kUnknown;
  }
  NOTREACHED();
}

// static
media::AudioLatency::Type
EnumTraits<media::mojom::AudioLatencyType, media::AudioLatency::Type>::
    FromMojom(media::mojom::AudioLatencyType input) {
  switch (input) {
    case media::mojom::AudioLatencyType::kExactMs:
      return media::AudioLatency::Type::kExactMS;
    case media::mojom::AudioLatencyType::kInteractive:
      return media::AudioLatency::Type::kInteractive;
    case media::mojom::AudioLatencyType::kRtc:
      return media::AudioLatency::Type::kRtc;
    case media::mojom::AudioLatencyType::kPlayback:
      return media::AudioLatency::Type::kPlayback;
    case media::mojom::AudioLatencyType::kUnknown:
      return media::AudioLatency::Type::kUnknown;
  }
  NOTREACHED();
}

// static
bool StructTraits<media::mojom::AudioParametersHardwareCapabilitiesDataView,
                  media::AudioParameters::HardwareCapabilities>::
    Read(media::mojom::AudioParametersHardwareCapabilitiesDataView input,
         media::AudioParameters::HardwareCapabilities* output) {
  if (input.min_frames_per_buffer() < 0 || input.max_frames_per_buffer() < 0 ||
      input.default_frames_per_buffer() < 0 || input.bitstream_formats() < 0) {
    return false;
  }
  output->min_frames_per_buffer = input.min_frames_per_buffer();
  output->max_frames_per_buffer = input.max_frames_per_buffer();
  output->default_frames_per_buffer = input.default_frames_per_buffer();
  output->bitstream_formats = input.bitstream_formats();
  output->require_encapsulation = input.require_encapsulation();
  output->require_audio_offload = input.require_audio_offload();
#if BUILDFLAG(IS_WIN)
  if (output->require_audio_offload &&
      !base::FeatureList::IsEnabled(media::kAudioOffload)) {
    return false;
  }
#endif
  return true;
}

// static
bool StructTraits<
    media::mojom::AudioParametersDataView,
    media::AudioParameters>::Read(media::mojom::AudioParametersDataView input,
                                  media::AudioParameters* output) {
  if (input.sample_rate() < 0 || input.frames_per_buffer() < 0) {
    return false;
  }

  media::AudioParameters::Format format;
  media::ChannelLayoutConfig channel_layout_config;
  std::vector<media::Point> mic_positions;
  media::AudioLatency::Type latency_tag;
  std::optional<media::AudioParameters::HardwareCapabilities>
      hardware_capabilities;

  if (!input.ReadFormat(&format) ||
      !input.ReadChannelLayoutConfig(&channel_layout_config) ||
      !input.ReadMicPositions(&mic_positions) ||
      !input.ReadLatencyTag(&latency_tag) ||
      !input.ReadHardwareCapabilities(&hardware_capabilities)) {
    return false;
  }

  *output =
      media::AudioParameters(format, channel_layout_config, input.sample_rate(),
                             input.frames_per_buffer());
  if (hardware_capabilities) {
    output->set_hardware_capabilities(hardware_capabilities);
  }

  // Forbid bitstream formats if passthrough is disabled.
#if !BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
  if (output->IsBitstreamFormat() ||
      (output->hardware_capabilities() &&
       output->hardware_capabilities()->bitstream_formats != 0)) {
    DLOG(ERROR) << "Failing AudioParameter serialization. Bitstream formats "
                   "are disabled.";
    return false;
  }
#endif

  output->set_effects(input.effects());
  output->set_mic_positions(std::move(mic_positions));
  output->set_latency_tag(latency_tag);

  return output->IsValid();
}

}  // namespace mojo
