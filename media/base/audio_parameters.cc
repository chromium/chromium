// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_parameters.h"

#include <sstream>

#include "media/base/audio_bus.h"
#include "media/base/limits.h"

namespace media {

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
    case AudioParameters::AUDIO_BITSTREAM_IEC61937:
      return "BITSTREAM_IEC61937";
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

AudioParameters::AudioParameters()
    : AudioParameters(AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_NONE, 0, 0) {}

AudioParameters::AudioParameters(Format format,
                                 ChannelLayout channel_layout,
                                 int sample_rate,
                                 int frames_per_buffer)
    : latency_tag_(AudioLatency::LATENCY_COUNT) {
  Reset(format, channel_layout, sample_rate, frames_per_buffer);
}

AudioParameters::AudioParameters(
    Format format,
    ChannelLayout channel_layout,
    int sample_rate,
    int frames_per_buffer,
    const HardwareCapabilities& hardware_capabilities)
    : latency_tag_(AudioLatency::LATENCY_COUNT),
      hardware_capabilities_(hardware_capabilities) {
  Reset(format, channel_layout, sample_rate, frames_per_buffer);
}

AudioParameters::~AudioParameters() = default;

AudioParameters::AudioParameters(const AudioParameters&) = default;
AudioParameters& AudioParameters::operator=(const AudioParameters&) = default;

void AudioParameters::Reset(Format format,
                            ChannelLayout channel_layout,
                            int sample_rate,
                            int frames_per_buffer) {
  format_ = format;
  channel_layout_ = channel_layout;
  channels_ = ChannelLayoutToChannelCount(channel_layout);
  sample_rate_ = sample_rate;
  frames_per_buffer_ = frames_per_buffer;
  effects_ = NO_EFFECTS;
  mic_positions_.clear();
}

bool AudioParameters::IsValid() const {
  return (channels_ > 0) && (channels_ <= media::limits::kMaxChannels) &&
         (channel_layout_ > CHANNEL_LAYOUT_UNSUPPORTED) &&
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
         (channel_layout_ == CHANNEL_LAYOUT_DISCRETE ||
          channels_ == ChannelLayoutToChannelCount(channel_layout_));
}

std::string AudioParameters::AsHumanReadableString() const {
  std::ostringstream s;
  s << "format: " << FormatToString(format())
    << ", channel_layout: " << channel_layout() << ", channels: " << channels()
    << ", sample_rate: " << sample_rate()
    << ", frames_per_buffer: " << frames_per_buffer()
    << ", effects: " << effects()
    << ", mic_positions: " << PointsToString(mic_positions_);
  if (hardware_capabilities_.has_value()) {
    s << ", hw_capabilities: min_frames_per_buffer: "
      << hardware_capabilities_->min_frames_per_buffer
      << ", max_frames_per_buffer: "
      << hardware_capabilities_->max_frames_per_buffer
      << ", bitstream_formats:" << hardware_capabilities_->bitstream_formats;
  }
  return s.str();
}

int AudioParameters::GetBytesPerBuffer(SampleFormat fmt) const {
  return GetBytesPerFrame(fmt) * frames_per_buffer_;
}

int AudioParameters::GetBytesPerFrame(SampleFormat fmt) const {
  return channels_ * SampleFormatToBytesPerChannel(fmt);
}

double AudioParameters::GetMicrosecondsPerFrame() const {
  return static_cast<double>(base::Time::kMicrosecondsPerSecond) / sample_rate_;
}

base::TimeDelta AudioParameters::GetBufferDuration() const {
  return base::Microseconds(static_cast<int64_t>(
      frames_per_buffer_ * base::Time::kMicrosecondsPerSecond /
      static_cast<float>(sample_rate_)));
}

bool AudioParameters::Equals(const AudioParameters& other) const {
  return format_ == other.format() && sample_rate_ == other.sample_rate() &&
         channel_layout_ == other.channel_layout() &&
         channels_ == other.channels() &&
         frames_per_buffer_ == other.frames_per_buffer() &&
         effects_ == other.effects() && mic_positions_ == other.mic_positions_;
}

bool AudioParameters::IsBitstreamFormat() const {
  switch (format_) {
    case AUDIO_BITSTREAM_AC3:
    case AUDIO_BITSTREAM_EAC3:
    case AUDIO_BITSTREAM_DTS:
    case AUDIO_BITSTREAM_DTS_HD:
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

// static
AudioParameters AudioParameters::UnavailableDeviceParams() {
  // Using 10 ms buffer since WebAudioMediaStreamSource::DeliverRebufferedAudio
  // deals incorrectly with reference time calculation if output buffer size
  // significantly differs from 10 ms used there, see http://crbug/701000.
  return media::AudioParameters(
      media::AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
      media::AudioParameters::kAudioCDSampleRate,
      media::AudioParameters::kAudioCDSampleRate / 100);
}

}  // namespace media
