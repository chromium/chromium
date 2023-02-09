// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/wav_audio_handler.h"

#include <algorithm>
#include <cstring>

#include "base/big_endian.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"

namespace media {
namespace {

const char kChunkId[] = "RIFF";
const char kFormat[] = "WAVE";
const char kFmtSubchunkId[] = "fmt ";
const char kDataSubchunkId[] = "data";

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
  WavAudioHandler::AudioFormat audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;
  uint16_t bits_per_sample;
  uint16_t valid_bits_per_sample;
  bool is_extensible;
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

// Reads an integer from |data| with |offset|.
template <typename T>
T ReadInt(const base::StringPiece& data, size_t offset) {
  CHECK_LE(offset + sizeof(T), data.size());
  T result;
  memcpy(&result, data.data() + offset, sizeof(T));
#if !defined(ARCH_CPU_LITTLE_ENDIAN)
  result = base::ByteSwap(result);
#endif
  return result;
}

// Parse a "fmt " chunk from wav data into its parameters.
bool ParseFmtChunk(const base::StringPiece data, WavAudioParameters* params) {
  DCHECK(params);

  // If the chunk is too small, return false.
  if (data.size() < kFmtChunkMinimumSize) {
    LOG(ERROR) << "Data size " << data.size() << " is too short.";
    return false;
  }

  // Read in serialized parameters.
  params->audio_format =
      ReadInt<WavAudioHandler::AudioFormat>(data, kAudioFormatOffset);
  params->num_channels = ReadInt<uint16_t>(data, kChannelOffset);
  params->sample_rate = ReadInt<uint32_t>(data, kSampleRateOffset);
  params->bits_per_sample = ReadInt<uint16_t>(data, kBitsPerSampleOffset);

  if (params->audio_format ==
      WavAudioHandler::AudioFormat::kAudioFormatExtensible) {
    if (data.size() < kFmtChunkExtensibleMinimumSize) {
      LOG(ERROR) << "Data size " << data.size() << " is too short.";
      return false;
    }

    params->is_extensible = true;
    params->audio_format =
        ReadInt<WavAudioHandler::AudioFormat>(data, kSubFormatOffset);
    params->valid_bits_per_sample =
        ReadInt<uint16_t>(data, kValidBitsPerSampleOffset);
  } else {
    params->is_extensible = false;
  }
  return true;
}

bool ParseWavData(const base::StringPiece wav_data,
                  base::StringPiece* audio_data_out,
                  WavAudioParameters* params_out) {
  DCHECK(audio_data_out);
  DCHECK(params_out);

  // The header should look like: |R|I|F|F|1|2|3|4|W|A|V|E|
  auto reader = base::BigEndianReader::FromStringPiece(wav_data);

  // Read the chunk ID and compare to "RIFF".
  base::StringPiece chunk_id;
  if (!reader.ReadPiece(&chunk_id, 4) || chunk_id != kChunkId) {
    DLOG(ERROR) << "missing or incorrect chunk ID in wav header";
    return false;
  }
  // The RIFF chunk length comes next, but we don't actually care what it says.
  if (!reader.Skip(sizeof(uint32_t))) {
    DLOG(ERROR) << "missing length in wav header";
    return false;
  }
  // Read format and compare to "WAVE".
  base::StringPiece format;
  if (!reader.ReadPiece(&format, 4) || format != kFormat) {
    DLOG(ERROR) << "missing or incorrect format ID in wav header";
    return false;
  }

  bool got_format = false;
  // If the number of remaining bytes is smaller than |kChunkHeaderSize|, it's
  // just junk at the end.
  while (reader.remaining() >= kChunkHeaderSize) {
    // We should be at the beginning of a subsection. The next 8 bytes are the
    // header and should look like: "|f|m|t| |1|2|3|4|" or "|d|a|t|a|1|2|3|4|".
    base::StringPiece chunk_fmt;
    uint32_t chunk_length;
    if (!reader.ReadPiece(&chunk_fmt, 4) || !reader.ReadU32(&chunk_length))
      break;
    chunk_length = base::ByteSwap(chunk_length);
    // Read |chunk_length| bytes of payload. If that is impossible, try to read
    // all remaining bytes as the payload.
    base::StringPiece chunk_payload;
    if (!reader.ReadPiece(&chunk_payload, chunk_length) &&
        !reader.ReadPiece(&chunk_payload, reader.remaining())) {
      break;
    }

    // Parse the subsection header, handling it if it is a "data" or "fmt "
    // chunk. Skip it otherwise.
    if (chunk_fmt == kFmtSubchunkId) {
      got_format = true;
      if (!ParseFmtChunk(chunk_payload, params_out))
        return false;
    } else if (chunk_fmt == kDataSubchunkId) {
      *audio_data_out = chunk_payload;
    } else {
      DVLOG(1) << "Skipping unknown data chunk: " << chunk_fmt << ".";
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

WavAudioHandler::WavAudioHandler(base::StringPiece audio_data,
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
    const base::StringPiece wav_data) {
  WavAudioParameters params;
  base::StringPiece audio_data;

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
          NOTREACHED()
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
          NOTREACHED()
              << "Unsupported bytes per sample encountered for float PCM: "
              << bytes_per_frame;
          bus->ZeroFrames(frames);
      }
      break;
    default:
      NOTREACHED() << "Unsupported audio format encountered: "
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
