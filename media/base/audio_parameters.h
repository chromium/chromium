// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_PARAMETERS_H_
#define MEDIA_BASE_AUDIO_PARAMETERS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_point.h"
#include "media/base/channel_layout.h"
#include "media/base/media_shmem_export.h"
#include "media/base/sample_format.h"

namespace media {

// Use a struct-in-struct approach to ensure that we can calculate the required
// size as sizeof(Audio{Input,Output}BufferParameters) + #(bytes in audio
// buffer) without using packing. Also align Audio{Input,Output}BufferParameters
// instead of in Audio{Input,Output}Buffer to be able to calculate size like so.
// Use a constexpr for the alignment value that's the same as
// AudioBus::kChannelAlignment, since MSVC doesn't accept the latter to be used.
#if BUILDFLAG(IS_WIN)
#pragma warning(push)
#pragma warning(disable : 4324)  // Disable warning for added padding.
#endif
constexpr int kParametersAlignment = 16;

// ****WARNING****: Do not change the field types or ordering of these fields
// without checking that alignment is correct. The structs may be concurrently
// accessed by both 32bit and 64bit process in shmem. http://crbug.com/781095.
struct MEDIA_SHMEM_EXPORT alignas(kParametersAlignment)
    AudioInputBufferParameters {
  double volume;
  int64_t capture_time_us;     // base::TimeTicks in microseconds.
  int64_t glitch_duration_us;  // base::TimeDelta in microseconds.
  uint32_t glitch_count;
  uint32_t size;
  uint32_t id;
  bool key_pressed;
};
struct MEDIA_SHMEM_EXPORT alignas(kParametersAlignment)
    AudioOutputBufferParameters {
  int64_t delay_us;            // base::TimeDelta in microseconds.
  int64_t delay_timestamp_us;  // base::TimeTicks in microseconds.
  int64_t glitch_duration_us;  // base::TimeDelta in microseconds.
  uint32_t glitch_count;
  uint32_t bitstream_data_size;
  uint32_t bitstream_frames;
};
#if BUILDFLAG(IS_WIN)
#pragma warning(pop)
#endif

struct MEDIA_SHMEM_EXPORT AudioInputBuffer {
  AudioInputBufferParameters params;
  int8_t audio[1];
};
struct MEDIA_SHMEM_EXPORT AudioOutputBuffer {
  AudioOutputBufferParameters params;
  int8_t audio[1];
};

struct MEDIA_SHMEM_EXPORT AudioRendererAlgorithmParameters {
  // The maximum size for the audio buffer.
  base::TimeDelta max_capacity;

  // The minimum size for the audio buffer.
  base::TimeDelta starting_capacity;

  // The minimum size for the audio buffer for encrypted streams.
  // Set this to be larger than |max_capacity| because the
  // performance of encrypted playback is always worse than clear playback, due
  // to decryption and potentially IPC overhead. For the context, see
  // https://crbug.com/403462, https://crbug.com/718161 and
  // https://crbug.com/879970.
  base::TimeDelta starting_capacity_for_encrypted;
};

class AudioParameters;

// These convenience function safely computes the size required for
// |shared_memory_count| AudioInputBuffers, with enough memory for AudioBus
// data, using |parameters| (or alternatively |channels| and |frames|). The
// functions not returning a CheckedNumeric will CHECK on overflow.
MEDIA_SHMEM_EXPORT base::CheckedNumeric<uint32_t>
ComputeAudioInputBufferSizeChecked(const AudioParameters& parameters,
                                   uint32_t audio_bus_count);

MEDIA_SHMEM_EXPORT uint32_t
ComputeAudioInputBufferSize(const AudioParameters& parameters,
                            uint32_t audio_bus_count);

MEDIA_SHMEM_EXPORT uint32_t
ComputeAudioInputBufferSize(int channels, int frames, uint32_t audio_bus_count);

// These convenience functions safely computes the size required for an
// AudioOutputBuffer with enough memory for AudioBus data using |parameters| (or
// alternatively |channels| and |frames|). The functions not returning a
// CheckedNumeric will CHECK on overflow.
MEDIA_SHMEM_EXPORT base::CheckedNumeric<uint32_t>
ComputeAudioOutputBufferSizeChecked(const AudioParameters& parameters);

MEDIA_SHMEM_EXPORT uint32_t
ComputeAudioOutputBufferSize(const AudioParameters& parameters);

MEDIA_SHMEM_EXPORT uint32_t ComputeAudioOutputBufferSize(int channels,
                                                         int frames);

// Channel count and ChannelLayout pair, with helper methods to enforce safe
// construction.
class MEDIA_SHMEM_EXPORT ChannelLayoutConfig {
 public:
  ChannelLayoutConfig(const ChannelLayoutConfig& other);
  ChannelLayoutConfig& operator=(const ChannelLayoutConfig& other);
  ChannelLayoutConfig();
  ChannelLayoutConfig(ChannelLayout channel_layout, int channels);
  ~ChannelLayoutConfig();

  template <ChannelLayout layout>
  static ChannelLayoutConfig FromLayout() {
    return ChannelLayoutConfig(layout, ChannelLayoutToChannelCount(layout));
  }

  static ChannelLayoutConfig Mono();

  static ChannelLayoutConfig Stereo();

  static ChannelLayoutConfig Guess(int channels);

  ChannelLayout channel_layout() const { return channel_layout_; }

  int channels() const { return channels_; }

 private:
  ChannelLayout channel_layout_;  // Order of surround sound channels.
  int channels_;                  // Number of channels.
};

// For |CHANNEL_LAYOUT_DISCRETE|, we have to explicitly set the number of
// channels, so we need to use the normal constructor.
template <>
ChannelLayoutConfig ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_DISCRETE>() =
    delete;

class MEDIA_SHMEM_EXPORT AudioParameters {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AudioEncodingFormat
  // GENERATED_JAVA_PREFIX_TO_STRIP: AUDIO_
  enum Format {
    AUDIO_FAKE = 0x000,               // Creates a fake AudioOutputStream object
    AUDIO_PCM_LINEAR = 0x001,         // PCM is 'raw' amplitude samples.
    AUDIO_PCM_LOW_LATENCY = 0x002,    // Linear PCM, low latency requested.
    AUDIO_BITSTREAM_AC3 = 0x004,      // Compressed AC3 bitstream.
    AUDIO_BITSTREAM_EAC3 = 0x008,     // Compressed E-AC3 bitstream.
    AUDIO_BITSTREAM_DTS = 0x010,      // Compressed DTS bitstream.
    AUDIO_BITSTREAM_DTS_HD = 0x020,   // Compressed DTS-HD bitstream.
    AUDIO_BITSTREAM_DTSX_P2 = 0x040,  // Compressed DTSX Profile 2 bitstream.
    AUDIO_BITSTREAM_IEC61937 = 0x080,  // Compressed IEC61937 bitstream.
    AUDIO_BITSTREAM_DTS_HD_MA =
        0x100,  // Compressed DTS-HD Master Audio bitstream.
    AUDIO_FORMAT_LAST =
        AUDIO_BITSTREAM_DTS_HD_MA,  // Only used for validation of format.
  };

  enum {
    // Telephone quality sample rate, mostly for speech-only audio.
    kTelephoneSampleRate = 8000,
    // CD sampling rate is 44.1 KHz or conveniently 2x2x3x3x5x5x7x7.
    kAudioCDSampleRate = 44100,
  };

  enum {
    // The maxmium number of PCM frames can be decoded out of a compressed
    // audio frame, e.g. MP3, AAC, AC-3.
    kMaxFramesPerCompressedAudioBuffer = 4096,
  };

  // Bitmasks to determine whether certain platform (typically hardware) audio
  // effects should be enabled.
  enum PlatformEffectsMask {
    NO_EFFECTS = 0x0,
    ECHO_CANCELLER = 1 << 0,
    DUCKING = 1 << 1,  // Enables ducking if the OS supports it.
    // KEYBOARD_MIC used to hold 1 << 2, but has been deprecated.
    HOTWORD = 1 << 3,
    NOISE_SUPPRESSION = 1 << 4,
    AUTOMATIC_GAIN_CONTROL = 1 << 5,
    EXPERIMENTAL_ECHO_CANCELLER = 1 << 6,  // Indicates an echo canceller is
                                           // available that should only
                                           // experimentally be enabled.
    MULTIZONE = 1 << 7,
    AUDIO_PREFETCH = 1 << 8,
    ALLOW_DSP_ECHO_CANCELLER = 1 << 9,
    ALLOW_DSP_NOISE_SUPPRESSION = 1 << 10,
    ALLOW_DSP_AUTOMATIC_GAIN_CONTROL = 1 << 11,

    FUCHSIA_RENDER_USAGE_BACKGROUND = 1 << 12,
    FUCHSIA_RENDER_USAGE_MEDIA = 1 << 13,
    FUCHSIA_RENDER_USAGE_INTERRUPTION = 1 << 14,
    FUCHSIA_RENDER_USAGE_SYSTEM_AGENT = 1 << 15,
    FUCHSIA_RENDER_USAGE_COMMUNICATION = 1 << 16,

    IGNORE_UI_GAINS = 1 << 17,

    VOICE_ISOLATION_SUPPORTED = 1 << 18,  // Set when system voice isolation is
                                          // supported.
    CLIENT_CONTROLLED_VOICE_ISOLATION =
        1 << 19,                // Set when client forces to
                                // enable/disable the platform voice
                                // isolation effects. False indicates
                                // to use platform default state.
    VOICE_ISOLATION = 1 << 20,  // Enable/Disable platform voice isolation.
                                // Only meaningful when
                                // CLIENT_CONTROLLED_VOICE_ISOLATION is set.
  };

  struct HardwareCapabilities {
    HardwareCapabilities(int min_frames_per_buffer,
                         int max_frames_per_buffer,
                         int default_frames_per_buffer,
                         bool require_offload)
        : min_frames_per_buffer(min_frames_per_buffer),
          max_frames_per_buffer(max_frames_per_buffer),
          default_frames_per_buffer(default_frames_per_buffer),
          require_audio_offload(require_offload) {}
    HardwareCapabilities(int min_frames_per_buffer, int max_frames_per_buffer)
        : min_frames_per_buffer(min_frames_per_buffer),
          max_frames_per_buffer(max_frames_per_buffer) {}
    HardwareCapabilities(int bitstream_formats, bool require_encapsulation)
        : bitstream_formats(bitstream_formats),
          require_encapsulation(require_encapsulation) {}
    HardwareCapabilities() = default;

    // Minimum and maximum buffer sizes supported by the audio hardware. Opening
    // a device with frames_per_buffer set to a value between min and max should
    // result in the audio hardware running close to this buffer size, values
    // above or below will be clamped to the min or max by the audio system.
    // Either value can be 0 and means that the min or max is not known.
    int min_frames_per_buffer = 0;
    int max_frames_per_buffer = 0;
    // The default buffer size that the device will use when frames_per_buffer
    // is not specified.  Can be `min_frames_per_buffer`,
    // `max_frames_per_buffer`, or a value in between.  Can be 0 when the
    // default is unknown.
    int default_frames_per_buffer = 0;
    // Bitstream formats (OR'ed) supported by audio hardware.
    int bitstream_formats = 0;
    // Bitstream will need to be encapsulated in IEC61937 to be
    // passed through to the audio hardware.
    bool require_encapsulation = false;
    // Require audio processing offload.
    bool require_audio_offload = false;
  };

  AudioParameters();

  AudioParameters(Format format,
                  ChannelLayoutConfig channel_layout_config,
                  int sample_rate,
                  int frames_per_buffer);

  AudioParameters(Format format,
                  ChannelLayoutConfig channel_layout_config,
                  int sample_rate,
                  int frames_per_buffer,
                  const HardwareCapabilities& hardware_capabilities);

  ~AudioParameters();

  // Re-initializes all members except for |hardware_capabilities_|.
  void Reset(Format format,
             ChannelLayoutConfig channel_layout_config,
             int sample_rate,
             int frames_per_buffer);

  // Checks that all values are in the expected range. All limits are specified
  // in media::Limits.
  bool IsValid() const;

  // Returns a human-readable string describing |*this|.  For debugging & test
  // output only.
  std::string AsHumanReadableString() const;

  // Returns size of audio buffer in bytes when using |fmt| for samples.
  int GetBytesPerBuffer(SampleFormat fmt) const;

  // Returns the number of bytes representing a frame of audio when using |fmt|
  // for samples.
  int GetBytesPerFrame(SampleFormat fmt) const;

  // Returns the number of microseconds per frame of audio. Intentionally
  // reported as a double to surface of partial microseconds per frame, which
  // is common for many sample rates. Failing to account for these nanoseconds
  // can lead to audio/video sync drift.
  double GetMicrosecondsPerFrame() const;

  // Returns the duration of this buffer as calculated from frames_per_buffer()
  // and sample_rate().
  base::TimeDelta GetBufferDuration() const;

  // Comparison with other AudioParams.
  bool Equals(const AudioParameters& other) const;

  // Return true if |format_| is compressed bitstream.
  bool IsBitstreamFormat() const;

  bool IsFormatSupportedByHardware(Format format) const;

  bool RequireEncapsulation() const;

  // Return true if offload is requested.
  bool RequireOffload() const;

  void set_format(Format format) { format_ = format; }
  Format format() const { return format_; }

  void SetChannelLayoutConfig(ChannelLayout layout, int channels);

  const ChannelLayoutConfig& channel_layout_config() const {
    return channel_layout_config_;
  }

  ChannelLayout channel_layout() const {
    return channel_layout_config_.channel_layout();
  }

  int channels() const { return channel_layout_config_.channels(); }

  void set_sample_rate(int sample_rate) { sample_rate_ = sample_rate; }
  int sample_rate() const { return sample_rate_; }

  void set_frames_per_buffer(int frames_per_buffer) {
    frames_per_buffer_ = frames_per_buffer;
  }
  int frames_per_buffer() const { return frames_per_buffer_; }

  std::optional<HardwareCapabilities> hardware_capabilities() const {
    return hardware_capabilities_;
  }

  void set_hardware_capabilities(
      const std::optional<HardwareCapabilities>& hwc) {
    hardware_capabilities_ = hwc;
  }

  void set_effects(int effects) { effects_ = effects; }
  int effects() const { return effects_; }

  void set_mic_positions(const std::vector<Point>& mic_positions) {
    mic_positions_ = mic_positions;
  }
  const std::vector<Point>& mic_positions() const { return mic_positions_; }

  void set_latency_tag(AudioLatency::Type latency_tag) {
    latency_tag_ = latency_tag;
  }
  AudioLatency::Type latency_tag() const { return latency_tag_; }

  AudioParameters(const AudioParameters&);
  AudioParameters& operator=(const AudioParameters&);

  // Creates reasonable dummy parameters in case no device is available.
  static AudioParameters UnavailableDeviceParams();

 private:
  Format format_;                              // Format of the stream.
  ChannelLayoutConfig channel_layout_config_;  // The channel layout and the
                                               // number of channels.
  int sample_rate_;                            // Sampling frequency/rate.
  int frames_per_buffer_;                      // Number of frames in a buffer.
  int effects_;  // Bitmask using PlatformEffectsMask.

  // Microphone positions using Cartesian coordinates:
  // x: the horizontal dimension, with positive to the right from the camera's
  //    perspective.
  // y: the depth dimension, with positive forward from the camera's
  //    perspective.
  // z: the vertical dimension, with positive upwards.
  //
  // Usually, the center of the microphone array will be treated as the origin
  // (often the position of the camera).
  //
  // An empty vector indicates unknown positions.
  std::vector<Point> mic_positions_;

  // Optional tag to pass latency info from renderer to browser. Set to
  // AudioLatency::LATENCY_COUNT by default, which means "not specified".
  AudioLatency::Type latency_tag_;

  // Audio hardware specific parameters, these are treated as read-only and
  // changing them has no effect.
  std::optional<HardwareCapabilities> hardware_capabilities_;
};

// Comparison is useful when AudioParameters is used with std structures.
inline bool operator<(const AudioParameters& a, const AudioParameters& b) {
  if (a.format() != b.format())
    return a.format() < b.format();
  if (a.channels() != b.channels())
    return a.channels() < b.channels();
  if (a.sample_rate() != b.sample_rate())
    return a.sample_rate() < b.sample_rate();
  return a.frames_per_buffer() < b.frames_per_buffer();
}

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_PARAMETERS_H_
