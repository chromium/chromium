// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/wav_audio_handler.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_view_util.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"

namespace media {
namespace {

constexpr auto kChunkId = std::to_array<uint8_t>({'R', 'I', 'F', 'F'});
constexpr auto kFormat = std::to_array<uint8_t>({'W', 'A', 'V', 'E'});
constexpr auto kFmtSubchunkId = std::to_array<uint8_t>({'f', 'm', 't', ' '});
constexpr auto kDataSubchunkId = std::to_array<uint8_t>({'d', 'a', 't', 'a'});

// The size of a chunk header in wav file format. A chunk header consists of a
// tag ('fmt ' or 'data') and 4 bytes of chunk length.
constexpr size_t kChunkHeaderSize = 8;

// The minimum size of 'fmt' chunk.
constexpr size_t kFmtChunkMinimumSize = 16;
constexpr size_t kFmtChunkExtensibleMinimumSize = 40;

// The offsets of 'fmt' fields.
constexpr size_t kAudioFormatOffset = 0;
constexpr size_t kChannelOffset = 2;
constexpr size_t kSampleRateOffset = 4;
constexpr size_t kBitsPerSampleOffset = 14;
constexpr size_t kValidBitsPerSampleOffset = 18;
constexpr size_t kSubFormatOffset = 24;

// A convenience class for passing WAV parameters around. AudioParameters is
// too heavyweight for this. Keep this class internal to this implementation.
class WavAudioParameters {
 public:
  WavAudioParameters() = default;

  WavAudioParameters(WavAudioHandler::AudioFormat audio_format,
                     uint16_t num_channels,
                     uint32_t sample_rate,
                     uint16_t bits_per_sample,
                     uint16_t valid_bits_per_sample = 0,
                     bool is_extensible = false)
      : audio_format_(audio_format),
        num_channels_(num_channels),
        sample_rate_(sample_rate),
        bits_per_sample_(bits_per_sample),
        valid_bits_per_sample_(valid_bits_per_sample),
        is_extensible_(is_extensible) {}

  WavAudioHandler::AudioFormat audio_format() const { return audio_format_; }
  uint16_t num_channels() const { return num_channels_; }
  uint32_t sample_rate() const { return sample_rate_; }
  uint16_t bits_per_sample() const { return bits_per_sample_; }
  uint16_t valid_bits_per_sample() const { return valid_bits_per_sample_; }
  bool is_extensible() const { return is_extensible_; }

  bool IsValid() const {
    return
        // Check number of channels
        num_channels_ != 0u &&
        num_channels_ <= static_cast<uint16_t>(limits::kMaxChannels) &&
        // Check sample rate
        sample_rate_ != 0u &&
        (
            // Check bits per second for PCM data
            (audio_format_ == WavAudioHandler::AudioFormat::kAudioFormatPCM &&
             (bits_per_sample_ == 8u || bits_per_sample_ == 16u ||
              bits_per_sample_ == 32u)) ||
            // Check bits per second for float data
            (audio_format_ == WavAudioHandler::AudioFormat::kAudioFormatFloat &&
             (bits_per_sample_ == 32u || bits_per_sample_ == 64u))) &&
        // Check extensible format bps
        (!is_extensible_ || valid_bits_per_sample_ == bits_per_sample_);
  }

 private:
  WavAudioHandler::AudioFormat audio_format_ =
      WavAudioHandler::AudioFormat::kAudioFormatPCM;
  uint16_t num_channels_ = 0;
  uint32_t sample_rate_ = 0;
  uint16_t bits_per_sample_ = 0;
  uint16_t valid_bits_per_sample_ = 0;
  bool is_extensible_ = false;
};

// Result struct containing both audio data and parameters from ParseWavData.
struct ParseWavResult {
  base::raw_span<const uint8_t> audio_data;
  WavAudioParameters params;
};

// Parse a "fmt " chunk from wav data into its parameters. The `data` is in
// little endian encoding.
std::optional<WavAudioParameters> ParseFmtChunk(
    base::span<const uint8_t> data) {
  // If the chunk is too small, return nullopt.
  if (data.size() < kFmtChunkMinimumSize) {
    DVLOG(1) << "Data size " << data.size() << " is too short.";
    return std::nullopt;
  }

  // Read in serialized parameters.
  WavAudioHandler::AudioFormat audio_format =
      static_cast<WavAudioHandler::AudioFormat>(base::U16FromLittleEndian(
          data.subspan<kAudioFormatOffset,
                       sizeof(WavAudioHandler::AudioFormat)>()));
  uint16_t num_channels =
      base::U16FromLittleEndian(data.subspan<kChannelOffset, 2u>());
  uint32_t sample_rate =
      base::U32FromLittleEndian(data.subspan<kSampleRateOffset, 4u>());
  uint16_t bits_per_sample =
      base::U16FromLittleEndian(data.subspan<kBitsPerSampleOffset, 2u>());

  WavAudioParameters params;

  if (audio_format == WavAudioHandler::AudioFormat::kAudioFormatExtensible) {
    if (data.size() < kFmtChunkExtensibleMinimumSize) {
      DVLOG(1) << "Data size " << data.size() << " is too short.";
      return std::nullopt;
    }

    WavAudioHandler::AudioFormat actual_format =
        static_cast<WavAudioHandler::AudioFormat>(base::U16FromLittleEndian(
            data.subspan<kSubFormatOffset,
                         sizeof(WavAudioHandler::AudioFormat)>()));
    uint16_t valid_bits_per_sample = base::U16FromLittleEndian(
        data.subspan<kValidBitsPerSampleOffset, 2u>());

    params = WavAudioParameters(actual_format, num_channels, sample_rate,
                                bits_per_sample, valid_bits_per_sample, true);
  } else {
    params = WavAudioParameters(audio_format, num_channels, sample_rate,
                                bits_per_sample);
  }

  // Validate the parameters before returning to ensure we only return
  // completely valid WavAudioParameters, never partially-valid results.
  if (!params.IsValid()) {
    DVLOG(1) << "Format chunk contains invalid parameters. "
             << "num_channels: " << params.num_channels() << " "
             << "sample_rate: " << params.sample_rate() << " "
             << "bits_per_sample: " << params.bits_per_sample() << " "
             << "valid_bits_per_sample: " << params.valid_bits_per_sample()
             << " "
             << "is_extensible: " << params.is_extensible();
    return std::nullopt;
  }

  return params;
}

// The `wav_data` is encoded in little endian, as will be the returned audio
// data.
std::optional<ParseWavResult> ParseWavData(base::span<const uint8_t> wav_data) {
  // The header should look like: |R|I|F|F|1|2|3|4|W|A|V|E|
  auto buf = base::SpanReader(wav_data);

  // Read the chunk ID and compare to "RIFF".
  std::optional<base::span<const uint8_t, 4u>> chunk_id = buf.Read<4u>();
  if (chunk_id != kChunkId) {
    DVLOG(1) << "missing or incorrect chunk ID in wav header";
    return std::nullopt;
  }
  // The RIFF chunk length comes next, but we don't actually care what it says.
  if (!buf.Skip(sizeof(uint32_t))) {
    DVLOG(1) << "missing length in wav header";
    return std::nullopt;
  }
  // Read format and compare to "WAVE".
  std::optional<base::span<const uint8_t, 4u>> format = buf.Read<4u>();
  if (format != kFormat) {
    DVLOG(1) << "missing or incorrect format ID in wav header";
    return std::nullopt;
  }

  ParseWavResult result;
  bool got_format = false;

  // If the number of remaining bytes is smaller than |kChunkHeaderSize|, it's
  // just junk at the end.
  while (buf.remaining() >= kChunkHeaderSize) {
    // We should be at the beginning of a subsection. The next 8 bytes are the
    // header and should look like: "|f|m|t| |1|2|3|4|" or "|d|a|t|a|1|2|3|4|".
    base::span<const uint8_t, 4u> chunk_fmt = *buf.Read<4u>();
    uint32_t chunk_length = base::U32FromLittleEndian(*buf.Read<4u>());

    // Read `chunk_length` bytes of payload. If that is impossible, read all
    // remaining bytes as the payload.
    base::span<const uint8_t> chunk_payload =
        *buf.Read(std::min(size_t{chunk_length}, buf.remaining()));

    // Parse the subsection header, handling it if it is a "data" or "fmt "
    // chunk. Skip it otherwise.
    if (chunk_fmt == kFmtSubchunkId) {
      got_format = true;
      std::optional<WavAudioParameters> params = ParseFmtChunk(chunk_payload);
      if (!params) {
        return std::nullopt;
      }
      result.params = std::move(*params);
    } else if (chunk_fmt == kDataSubchunkId) {
      result.audio_data = chunk_payload;
    } else {
      DVLOG(1) << "Skipping unknown data chunk: "
               << base::as_string_view(chunk_fmt) << ".";
    }
  }

  // Check that data format has been read in.
  if (!got_format) {
    DVLOG(1) << "Invalid: No \"" << base::as_string_view(kFmtSubchunkId)
             << "\" header found!";
    return std::nullopt;
  }
  return result;
}

}  // namespace

WavAudioHandler::WavAudioHandler(base::span<const uint8_t> audio_data,
                                 uint16_t num_channels,
                                 uint32_t sample_rate,
                                 uint16_t bits_per_sample,
                                 AudioFormat audio_format)
    : audio_data_(audio_data),
      num_channels_(num_channels),
      sample_rate_(sample_rate),
      bits_per_sample_(bits_per_sample),
      audio_format_(audio_format) {
  DCHECK_NE(num_channels_, 0u);
  DCHECK_NE(sample_rate_, 0u);
  DCHECK_NE(bits_per_sample_, 0u);
  total_frames_ = audio_data_.size() * 8 / num_channels_ / bits_per_sample_;
}
WavAudioHandler::~WavAudioHandler() = default;

// static
std::unique_ptr<WavAudioHandler> WavAudioHandler::Create(
    base::span<const uint8_t> wav_data) {
  // Attempt to parse the WAV data.
  std::optional<ParseWavResult> result = ParseWavData(wav_data);
  if (!result) {
    return nullptr;
  }

  return base::WrapUnique(new WavAudioHandler(
      result->audio_data, result->params.num_channels(),
      result->params.sample_rate(), result->params.bits_per_sample(),
      result->params.audio_format()));
}

bool WavAudioHandler::Initialize() {
  return true;
}

int WavAudioHandler::GetNumChannels() const {
  return static_cast<int>(num_channels_);
}

int WavAudioHandler::GetSampleRate() const {
  return static_cast<int>(sample_rate_);
}

bool WavAudioHandler::AtEnd() const {
  return audio_data_.size() <= cursor_;
}

bool WavAudioHandler::CopyTo(AudioBus* bus, size_t* frames_written) {
  DCHECK(bus);
  DCHECK_EQ(bus->channels(), num_channels_);

  if (AtEnd()) {
    bus->Zero();
    *frames_written = 0;
    return true;
  }
  const int bytes_per_frame = num_channels_ * bits_per_sample_ / 8;
  const int remaining_frames = (audio_data_.size() - cursor_) / bytes_per_frame;
  const int frames = std::min(bus->frames(), remaining_frames);
  const auto* source = audio_data_.subspan(cursor_).data();

  switch (audio_format_) {
    case AudioFormat::kAudioFormatPCM:
      switch (bits_per_sample_) {
        case 8:
          bus->FromInterleaved<UnsignedInt8SampleTypeTraits>(
              reinterpret_cast<const uint8_t*>(source), frames);
          break;
        case 16:
          bus->FromInterleaved<SignedInt16SampleTypeTraits>(
              reinterpret_cast<const int16_t*>(source), frames);
          break;
        case 32:
          bus->FromInterleaved<SignedInt32SampleTypeTraits>(
              reinterpret_cast<const int32_t*>(source), frames);
          break;
        default:
          NOTREACHED()
              << "Unsupported bytes per sample encountered for integer PCM: "
              << bytes_per_frame;
      }
      break;
    case AudioFormat::kAudioFormatFloat:
      switch (bits_per_sample_) {
        case 32:
          bus->FromInterleaved<Float32SampleTypeTraitsNoClip>(
              reinterpret_cast<const float*>(source), frames);
          break;
        case 64:
          bus->FromInterleaved<Float64SampleTypeTraits>(
              reinterpret_cast<const double*>(source), frames);
          break;
        default:
          NOTREACHED()
              << "Unsupported bytes per sample encountered for float PCM: "
              << bytes_per_frame;
      }
      break;
    default:
      NOTREACHED() << "Unsupported audio format encountered: "
                   << static_cast<uint16_t>(audio_format_);
  }
  *frames_written = frames;
  cursor_ += frames * bytes_per_frame;
  bus->ZeroFramesPartial(frames, bus->frames() - frames);
  return true;
}

base::TimeDelta WavAudioHandler::GetDuration() const {
  return AudioTimestampHelper::FramesToTime(total_frames_, sample_rate_);
}

void WavAudioHandler::Reset() {
  cursor_ = 0;
}

}  // namespace media
