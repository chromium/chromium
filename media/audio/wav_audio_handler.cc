// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/wav_audio_handler.h"

#include <algorithm>
#include <cstring>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"

namespace media {
namespace {

const uint8_t kChunkId[] = {'R', 'I', 'F', 'F'};
const uint8_t kFormat[] = {'W', 'A', 'V', 'E'};
const uint8_t kFmtSubchunkId[] = {'f', 'm', 't', ' '};
const uint8_t kDataSubchunkId[] = {'d', 'a', 't', 'a'};

// The size of a chunk header in wav file format. A chunk header consists of a
// tag ('fmt ' or 'data') and 4 bytes of chunk length.
const size_t kChunkHeaderSize = 8;

// The minimum size of 'fmt' chunk.
const size_t kFmtChunkMinimumSize = 16;
const size_t kFmtChunkExtensibleMinimumSize = 40;

// The offsets of 'fmt' fields.
const size_t kAudioFormatOffset = 0;
const size_t kChannelOffset = 2;
const size_t kSampleRateOffset = 4;
const size_t kBitsPerSampleOffset = 14;
const size_t kValidBitsPerSampleOffset = 18;
const size_t kSubFormatOffset = 24;

// A convenience struct for passing WAV parameters around. AudioParameters is
// too heavyweight for this. Keep this class internal to this implementation.
struct WavAudioParameters {
  // TODO(crbug.com/340824112): note that zero-initializing this field does not
  // correspond to any enumerator value defined in the `AudioFormat` enum.
  // However, not initializing is also problematic: `ParseFmtChunk()` simply
  // early-returns on failure, leaving random fields uninitialized and causing
  // MSan errors elsewhere. A better long-term solution would be for
  // `ParseFmtChunk` to return a `std::optional<WavAudioParameters>`. For now,
  // zero-initializing this field has a (small) benefit that it won't
  // correspond to any valid format and is guaranteed to fail the
  // `ParamsAreValid()` check.
  WavAudioHandler::AudioFormat audio_format = {};
  uint16_t num_channels = 0;
  uint32_t sample_rate = 0;
  uint16_t bits_per_sample = 0;
  uint16_t valid_bits_per_sample = 0;
  bool is_extensible = false;
};

bool ParamsAreValid(const WavAudioParameters& params) {
  return (
      // Check number of channels
      params.num_channels != 0u &&
      params.num_channels <= static_cast<uint16_t>(limits::kMaxChannels) &&
      // Check sample rate
      params.sample_rate != 0u &&
      (
          // Check bits per second for PCM data
          (params.audio_format ==
               WavAudioHandler::AudioFormat::kAudioFormatPCM &&
           (params.bits_per_sample == 8u || params.bits_per_sample == 16u ||
            params.bits_per_sample == 32u)) ||
          // Check bits per second for float data
          (params.audio_format ==
               WavAudioHandler::AudioFormat::kAudioFormatFloat &&
           (params.bits_per_sample == 32u || params.bits_per_sample == 64u))) &&
      // Check extensible format bps
      (!params.is_extensible ||
       params.valid_bits_per_sample == params.bits_per_sample));
}

// Parse a "fmt " chunk from wav data into its parameters. The `data` is in
// little endian encoding.
bool ParseFmtChunk(base::span<const uint8_t> data, WavAudioParameters* params) {
  DCHECK(params);

  // If the chunk is too small, return false.
  if (data.size() < kFmtChunkMinimumSize) {
    LOG(ERROR) << "Data size " << data.size() << " is too short.";
    return false;
  }

  // Read in serialized parameters.
  params->audio_format =
      static_cast<WavAudioHandler::AudioFormat>(base::U16FromLittleEndian(
          data.subspan<kAudioFormatOffset,
                       sizeof(WavAudioHandler::AudioFormat)>()));
  params->num_channels =
      base::U16FromLittleEndian(data.subspan<kChannelOffset, 2u>());
  params->sample_rate =
      base::U32FromLittleEndian(data.subspan<kSampleRateOffset, 4u>());
  params->bits_per_sample =
      base::U16FromLittleEndian(data.subspan<kBitsPerSampleOffset, 2u>());

  if (params->audio_format ==
      WavAudioHandler::AudioFormat::kAudioFormatExtensible) {
    if (data.size() < kFmtChunkExtensibleMinimumSize) {
      LOG(ERROR) << "Data size " << data.size() << " is too short.";
      return false;
    }

    params->is_extensible = true;
    params->audio_format =
        static_cast<WavAudioHandler::AudioFormat>(base::U16FromLittleEndian(
            data.subspan<kSubFormatOffset,
                         sizeof(WavAudioHandler::AudioFormat)>()));
    params->valid_bits_per_sample = base::U16FromLittleEndian(
        data.subspan<kValidBitsPerSampleOffset, 2u>());
  } else {
    params->is_extensible = false;
  }
  return true;
}

// The `wav_data` is encoded in little endian, as will be `audio_data_out`.
bool ParseWavData(std::string_view wav_data,
                  std::string_view* audio_data_out,
                  WavAudioParameters* params_out) {
  DCHECK(audio_data_out);
  DCHECK(params_out);

  // The header should look like: |R|I|F|F|1|2|3|4|W|A|V|E|
  auto buf = base::SpanReader(base::as_byte_span(wav_data));

  // Read the chunk ID and compare to "RIFF".
  std::optional<base::span<const uint8_t, 4u>> chunk_id = buf.Read<4u>();
  if (chunk_id != kChunkId) {
    DLOG(ERROR) << "missing or incorrect chunk ID in wav header";
    return false;
  }
  // The RIFF chunk length comes next, but we don't actually care what it says.
  if (!buf.Skip(sizeof(uint32_t))) {
    DLOG(ERROR) << "missing length in wav header";
    return false;
  }
  // Read format and compare to "WAVE".
  std::optional<base::span<const uint8_t, 4u>> format = buf.Read<4u>();
  if (format != kFormat) {
    DLOG(ERROR) << "missing or incorrect format ID in wav header";
    return false;
  }

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
      if (!ParseFmtChunk(chunk_payload, params_out))
        return false;
    } else if (chunk_fmt == kDataSubchunkId) {
      *audio_data_out = base::as_string_view(chunk_payload);
    } else {
      DVLOG(1) << "Skipping unknown data chunk: "
               << base::as_string_view(chunk_fmt) << ".";
    }
  }

  // Check that data format has been read in and is valid.
  if (!got_format) {
    LOG(ERROR) << "Invalid: No \"" << kFmtSubchunkId << "\" header found!";
    return false;
  } else if (!ParamsAreValid(*params_out)) {
    LOG(ERROR) << "Format is invalid. "
               << "num_channels: " << params_out->num_channels << " "
               << "sample_rate: " << params_out->sample_rate << " "
               << "bits_per_sample: " << params_out->bits_per_sample << " "
               << "valid_bits_per_sample: " << params_out->valid_bits_per_sample
               << " "
               << "is_extensible: " << params_out->is_extensible;
    return false;
  }
  return true;
}

}  // namespace

WavAudioHandler::WavAudioHandler(std::string_view audio_data,
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
    std::string_view wav_data) {
  WavAudioParameters params;
  std::string_view audio_data;

  // Attempt to parse the WAV data.
  if (!ParseWavData(wav_data, &audio_data, &params))
    return nullptr;

  return base::WrapUnique(
      new WavAudioHandler(audio_data, params.num_channels, params.sample_rate,
                          params.bits_per_sample, params.audio_format));
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
    return true;
  }
  const int bytes_per_frame = num_channels_ * bits_per_sample_ / 8;
  const int remaining_frames = (audio_data_.size() - cursor_) / bytes_per_frame;
  const int frames = std::min(bus->frames(), remaining_frames);
  const auto* source = audio_data_.data() + cursor_;

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
          NOTREACHED_IN_MIGRATION()
              << "Unsupported bytes per sample encountered for integer PCM: "
              << bytes_per_frame;
          bus->ZeroFrames(frames);
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
          NOTREACHED_IN_MIGRATION()
              << "Unsupported bytes per sample encountered for float PCM: "
              << bytes_per_frame;
          bus->ZeroFrames(frames);
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported audio format encountered: "
                                << static_cast<uint16_t>(audio_format_);
      bus->ZeroFrames(frames);
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
