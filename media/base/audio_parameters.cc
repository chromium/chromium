// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_parameters.h"

#include <sstream>

#include "base/check_op.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_latency.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"

namespace media {

AudioOutputBufferParametersHelper::AudioOutputBufferParametersHelper() =
    default;
AudioOutputBufferParametersHelper::~AudioOutputBufferParametersHelper() =
    default;
AudioGlitchInfo
AudioOutputBufferParametersHelper::GetGlitchIncrementSinceLastCall(
    AudioOutputBufferParameters& params) {
  base::TimeDelta current_glitch_duration = base::Microseconds(
      std::atomic_ref<int64_t>(params.cumulative_glitch_duration_us)
          .load(std::memory_order_relaxed));
  uint64_t current_glitch_count =
      std::atomic_ref<uint64_t>(params.cumulative_glitch_count)
          .load(std::memory_order_relaxed);

  DCHECK_GE(current_glitch_duration, previous_glitch_duration_);
  DCHECK_GE(current_glitch_count, previous_glitch_count_);

  media::AudioGlitchInfo glitch_info{
      .duration = current_glitch_duration - previous_glitch_duration_,
      .count = base::saturated_cast<uint32_t>(current_glitch_count -
                                              previous_glitch_count_)};
  previous_glitch_duration_ = current_glitch_duration;
  previous_glitch_count_ = current_glitch_count;
  return glitch_info;
}

// static
void AudioOutputBufferParametersHelper::AddGlitchIncrementToBuffer(
    AudioOutputBufferParameters& params,
    AudioGlitchInfo glitch_info) {
  std::atomic_ref<int64_t>(params.cumulative_glitch_duration_us)
      .fetch_add(glitch_info.duration.InMicroseconds(),
                 std::memory_order_relaxed);
  std::atomic_ref<uint64_t>(params.cumulative_glitch_count)
      .fetch_add(glitch_info.count, std::memory_order_relaxed);
}

static_assert(AudioBus::kChannelAlignment == kParametersAlignment,
              "Audio buffer parameters struct alignment not same as AudioBus");
static_assert(sizeof(AudioInputBufferParameters) %
                      AudioBus::kChannelAlignment ==
                  0,
              "AudioInputBufferParameters not aligned");
static_assert(sizeof(AudioOutputBufferParameters) %
                      AudioBus::kChannelAlignment ==
                  0,
              "AudioOutputBufferParameters not aligned");

const char* FormatToString(AudioParameters::Format format) {
  switch (format) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      return "PCM_LINEAR";
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      return "PCM_LOW_LATENCY";
    case AudioParameters::AUDIO_BITSTREAM_AC3:
      return "BITSTREAM_AC3";
    case AudioParameters::AUDIO_BITSTREAM_EAC3:
      return "BITSTREAM_EAC3";
    case AudioParameters::AUDIO_BITSTREAM_DTS:
      return "BITSTREAM_DTS";
    case AudioParameters::AUDIO_BITSTREAM_DTS_HD:
      return "BITSTREAM_DTS_HD";
    case AudioParameters::AUDIO_BITSTREAM_DTSX_P2:
      return "BITSTREAM_DTSX_P2";
    case AudioParameters::AUDIO_BITSTREAM_IEC61937:
      return "BITSTREAM_IEC61937";
    case AudioParameters::AUDIO_BITSTREAM_DTS_HD_MA:
      return "BITSTREAM_DTS_HD_MA";
    case AudioParameters::AUDIO_FAKE:
      return "FAKE";
  }
}

base::CheckedNumeric<uint32_t> ComputeAudioInputBufferSizeChecked(
    const AudioParameters& parameters,
    uint32_t shared_memory_count) {
  base::CheckedNumeric<uint32_t> result = AudioBus::CalculateMemorySize(
      parameters.channels(), parameters.frames_per_buffer());
  result += sizeof(media::AudioInputBufferParameters);
  result *= shared_memory_count;
  return result;
}

uint32_t ComputeAudioInputBufferSize(const AudioParameters& parameters,
                                     uint32_t shared_memory_count) {
  return ComputeAudioInputBufferSizeChecked(parameters, shared_memory_count)
      .ValueOrDie();
}

uint32_t ComputeAudioInputBufferSize(int channels,
                                     int frames,
                                     uint32_t shared_memory_count) {
  base::CheckedNumeric<uint32_t> result =
      AudioBus::CalculateMemorySize(channels, frames);
  result += sizeof(media::AudioInputBufferParameters);
  result *= shared_memory_count;
  return result.ValueOrDie();
}

base::CheckedNumeric<uint32_t> ComputeAudioOutputBufferSizeChecked(
    const AudioParameters& parameters) {
  base::CheckedNumeric<uint32_t> result = AudioBus::CalculateMemorySize(
      parameters.channels(), parameters.frames_per_buffer());
  result += sizeof(media::AudioOutputBufferParameters);
  return result;
}

uint32_t ComputeAudioOutputBufferSize(const AudioParameters& parameters) {
  return ComputeAudioOutputBufferSize(parameters.channels(),
                                      parameters.frames_per_buffer());
}

uint32_t ComputeAudioOutputBufferSize(int channels, int frames) {
  base::CheckedNumeric<uint32_t> result =
      AudioBus::CalculateMemorySize(channels, frames);
  result += sizeof(media::AudioOutputBufferParameters);
  return result.ValueOrDie();
}

// static
std::string AudioParameters::EffectsMaskToString(int mask) {
  if (mask == AudioParameters::NO_EFFECTS) {
    return "NONE";
  }

  std::vector<std::string> effects;
  if (mask & AudioParameters::ECHO_CANCELLER) {
    effects.push_back("ECHO_CANCELLER");
  }
  if (mask & AudioParameters::DUCKING) {
    effects.push_back("DUCKING");
  }
  if (mask & AudioParameters::HOTWORD) {
    effects.push_back("HOTWORD");
  }
  if (mask & AudioParameters::NOISE_SUPPRESSION) {
    effects.push_back("NOISE_SUPPRESSION");
  }
  if (mask & AudioParameters::AUTOMATIC_GAIN_CONTROL) {
    effects.push_back("AUTOMATIC_GAIN_CONTROL");
  }
  if (mask & AudioParameters::MULTIZONE) {
    effects.push_back("MULTIZONE");
  }
  if (mask & AudioParameters::AUDIO_PREFETCH) {
    effects.push_back("AUDIO_PREFETCH");
  }
  if (mask & AudioParameters::ALLOW_DSP_ECHO_CANCELLER) {
    effects.push_back("ALLOW_DSP_ECHO_CANCELLER");
  }
  if (mask & AudioParameters::ALLOW_DSP_NOISE_SUPPRESSION) {
    effects.push_back("ALLOW_DSP_NOISE_SUPPRESSION");
  }
  if (mask & AudioParameters::ALLOW_DSP_AUTOMATIC_GAIN_CONTROL) {
    effects.push_back("ALLOW_DSP_AUTOMATIC_GAIN_CONTROL");
  }
  if (mask & AudioParameters::FUCHSIA_RENDER_USAGE_BACKGROUND) {
    effects.push_back("FUCHSIA_RENDER_USAGE_BACKGROUND");
  }
  if (mask & AudioParameters::FUCHSIA_RENDER_USAGE_MEDIA) {
    effects.push_back("FUCHSIA_RENDER_USAGE_MEDIA");
  }
  if (mask & AudioParameters::FUCHSIA_RENDER_USAGE_INTERRUPTION) {
    effects.push_back("FUCHSIA_RENDER_USAGE_INTERRUPTION");
  }
  if (mask & AudioParameters::FUCHSIA_RENDER_USAGE_SYSTEM_AGENT) {
    effects.push_back("FUCHSIA_RENDER_USAGE_SYSTEM_AGENT");
  }
  if (mask & AudioParameters::FUCHSIA_RENDER_USAGE_COMMUNICATION) {
    effects.push_back("FUCHSIA_RENDER_USAGE_COMMUNICATION");
  }
  if (mask & AudioParameters::IGNORE_UI_GAINS) {
    effects.push_back("IGNORE_UI_GAINS");
  }
  if (mask & AudioParameters::VOICE_ISOLATION_SUPPORTED) {
    effects.push_back("VOICE_ISOLATION_SUPPORTED");
  }
  if (mask & AudioParameters::CLIENT_CONTROLLED_VOICE_ISOLATION) {
    effects.push_back("CLIENT_CONTROLLED_VOICE_ISOLATION");
  }
  if (mask & AudioParameters::VOICE_ISOLATION) {
    effects.push_back("VOICE_ISOLATION");
  }
  if (mask & AudioParameters::DEEP_NOISE_SUPPRESSION) {
    effects.push_back("WINDOWS_DEEP_NOISE_SUPPRESSION");
  }

  std::string result;
  for (size_t i = 0; i < effects.size(); ++i) {
    if (i > 0) {
      result += " | ";
    }
    result += effects[i];
  }
  return result;
}

AudioParameters::AudioParameters()
    : AudioParameters(AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_NONE>(),
                      0,
                      0) {}

AudioParameters::AudioParameters(Format format,
                                 ChannelLayoutConfig channel_layout_config,
                                 int sample_rate,
                                 int frames_per_buffer)
    : latency_tag_(AudioLatency::Type::kUnknown) {
  Reset(format, channel_layout_config, sample_rate, frames_per_buffer);
}

AudioParameters::AudioParameters(
    Format format,
    ChannelLayoutConfig channel_layout_config,
    int sample_rate,
    int frames_per_buffer,
    const HardwareCapabilities& hardware_capabilities)
    : latency_tag_(AudioLatency::Type::kUnknown),
      hardware_capabilities_(hardware_capabilities) {
  Reset(format, channel_layout_config, sample_rate, frames_per_buffer);
}

AudioParameters::~AudioParameters() = default;

AudioParameters::AudioParameters(const AudioParameters&) = default;
AudioParameters& AudioParameters::operator=(const AudioParameters&) = default;

void AudioParameters::Reset(Format format,
                            ChannelLayoutConfig channel_layout_config,
                            int sample_rate,
                            int frames_per_buffer) {
  format_ = format;
  channel_layout_config_ = channel_layout_config;
  sample_rate_ = sample_rate;
  frames_per_buffer_ = frames_per_buffer;
  effects_ = NO_EFFECTS;
  mic_positions_.clear();
}

bool AudioParameters::IsValid() const {
  return (channels() > 0) && (channels() <= media::limits::kMaxChannels) &&
         (channel_layout() > CHANNEL_LAYOUT_UNSUPPORTED) &&
         (sample_rate_ >= media::limits::kMinSampleRate) &&
         (sample_rate_ <= media::limits::kMaxSampleRate) &&
         (frames_per_buffer_ > 0) &&
         (frames_per_buffer_ <= media::limits::kMaxSamplesPerPacket) &&
         (!hardware_capabilities_ ||
          ((hardware_capabilities_->min_frames_per_buffer >= 0) &&
           (hardware_capabilities_->min_frames_per_buffer <=
            media::limits::kMaxSamplesPerPacket) &&
           (hardware_capabilities_->max_frames_per_buffer >= 0) &&
           (hardware_capabilities_->max_frames_per_buffer <=
            media::limits::kMaxSamplesPerPacket) &&
           (hardware_capabilities_->max_frames_per_buffer >=
            hardware_capabilities_->min_frames_per_buffer))) &&
         (channel_layout() == CHANNEL_LAYOUT_DISCRETE ||
          channel_layout() == CHANNEL_LAYOUT_5_1_4_DOWNMIX ||
          channels() == ChannelLayoutToChannelCount(channel_layout()));
}

std::string AudioParameters::AsHumanReadableString() const {
  std::ostringstream s;
  s << "format: " << FormatToString(format())
    << ", channel_layout: " << channel_layout() << ", channels: " << channels()
    << ", sample_rate: " << sample_rate()
    << ", frames_per_buffer: " << frames_per_buffer()
    << ", effects: " << AudioParameters::EffectsMaskToString(effects())
    << ", mic_positions: " << PointsToString(mic_positions_)
    << ", latency_tag: " << AudioLatency::ToString(latency_tag());
  if (hardware_capabilities_.has_value()) {
    s << ", hw_capabilities: min_frames_per_buffer: "
      << hardware_capabilities_->min_frames_per_buffer
      << ", max_frames_per_buffer: "
      << hardware_capabilities_->max_frames_per_buffer
      << ", bitstream_formats:" << hardware_capabilities_->bitstream_formats
      << ", require_encapsulation:"
      << hardware_capabilities_->require_encapsulation
      << ", require_audio_offload:"
      << hardware_capabilities_->require_audio_offload;
  }
  return s.str();
}

int AudioParameters::GetBytesPerBuffer(SampleFormat fmt) const {
  return base::CheckMul(GetBytesPerFrame(fmt), frames_per_buffer_)
      .ValueOrDie<int>();
}

int AudioParameters::GetBytesPerFrame(SampleFormat fmt) const {
  return channels() * SampleFormatToBytesPerChannel(fmt);
}

base::TimeDelta AudioParameters::GetBufferDuration() const {
  return base::Microseconds(static_cast<int64_t>(
      frames_per_buffer_ * base::Time::kMicrosecondsPerSecond /
      static_cast<float>(sample_rate_)));
}

bool AudioParameters::Equals(const AudioParameters& other) const {
  return format_ == other.format() && sample_rate_ == other.sample_rate() &&
         channel_layout() == other.channel_layout() &&
         channels() == other.channels() &&
         frames_per_buffer_ == other.frames_per_buffer() &&
         effects_ == other.effects() && mic_positions_ == other.mic_positions_;
}

bool AudioParameters::IsBitstreamFormat() const {
  switch (format_) {
    case AUDIO_BITSTREAM_AC3:
    case AUDIO_BITSTREAM_EAC3:
    case AUDIO_BITSTREAM_DTS:
    case AUDIO_BITSTREAM_DTS_HD:
    case AUDIO_BITSTREAM_DTSX_P2:
    case AUDIO_BITSTREAM_IEC61937:
      return true;
    default:
      return false;
  }
}

bool AudioParameters::IsFormatSupportedByHardware(Format format) const {
  return hardware_capabilities_.has_value() &&
         (hardware_capabilities_->bitstream_formats & format);
}

void AudioParameters::SetChannelLayoutConfig(ChannelLayout layout,
                                             int channels) {
  channel_layout_config_ = {layout, channels};
}

bool AudioParameters::RequireEncapsulation() const {
  return hardware_capabilities_.has_value() &&
         hardware_capabilities_->require_encapsulation;
}

bool AudioParameters::RequireOffload() const {
  return hardware_capabilities_.has_value() &&
         hardware_capabilities_->require_audio_offload;
}

// static
AudioParameters AudioParameters::UnavailableDeviceParams() {
  // Using 10 ms buffer since WebAudioMediaStreamSource::DeliverRebufferedAudio
  // deals incorrectly with reference time calculation if output buffer size
  // significantly differs from 10 ms used there, see http://crbug/701000.
  return media::AudioParameters(
      media::AudioParameters::AUDIO_FAKE, ChannelLayoutConfig::Stereo(),
      media::AudioParameters::kAudioCDSampleRate,
      media::AudioParameters::kAudioCDSampleRate / 100);
}

}  // namespace media
