// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/wav_audio_handler.h"

#include <algorithm>
#include <cstring>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"

namespace media {
namespace {

const char kChunkId[] = "RIFF";
const char kFormat[] = "WAVE";
const char kFmtSubchunkId[] = "fmt ";
const char kDataSubchunkId[] = "data";

// The size of the header of a wav file. The header consists of 'RIFF', 4 bytes
// of total data length, and 'WAVE'.
const size_t kWavFileHeaderSize = 12;

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

// Some constants for audio format.
const int kAudioFormatPCM = 1;
const int kAudioFormatExtensible = 0xfffe;

// A convenience struct for passing WAV parameters around. AudioParameters is
// too heavyweight for this. Keep this class internal to this implementation.
struct WavAudioParameters {
  int audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;
  uint16_t bits_per_sample;
  uint16_t valid_bits_per_sample;
  bool is_extensible;
};

bool ParamsAreValid(const WavAudioParameters& params) {
  return (params.audio_format == kAudioFormatPCM && params.num_channels != 0u &&
          params.sample_rate != 0u && params.bits_per_sample != 0u &&
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
  params->audio_format = ReadInt<uint16_t>(data, kAudioFormatOffset);
  params->num_channels = ReadInt<uint16_t>(data, kChannelOffset);
  params->sample_rate = ReadInt<uint32_t>(data, kSampleRateOffset);
  params->bits_per_sample = ReadInt<uint16_t>(data, kBitsPerSampleOffset);

  if (params->audio_format == kAudioFormatExtensible) {
    if (data.size() < kFmtChunkExtensibleMinimumSize) {
      LOG(ERROR) << "Data size " << data.size() << " is too short.";
      return false;
    }

    params->is_extensible = true;
    params->audio_format = ReadInt<uint16_t>(data, kSubFormatOffset);
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

  // The data is not long enough to contain a header.
  if (wav_data.size() < kWavFileHeaderSize) {
    LOG(ERROR) << "wav_data is too small";
    return false;
  }

  // The header should look like: |R|I|F|F|1|2|3|4|W|A|V|E|
  if (!wav_data.starts_with(kChunkId) ||
      memcmp(wav_data.data() + 8, kFormat, 4) != 0) {
    LOG(ERROR) << "incorrect wav header";
    return false;
  }

  // Get the total length of the data. This number should reflect the total
  // number of valid bytes in |wav_data|. Read this from the header and add
  // 8 (4 for "RIFF" and 4 for the size itself), and if that is too big, use
  // the length of |wav_data|. We will attempt to parse the data.
  uint32_t total_length = std::min(ReadInt<uint32_t>(wav_data, 4) + 8,
                                   static_cast<uint32_t>(wav_data.size()));

  uint32_t offset = kWavFileHeaderSize;
  bool got_format = false;
  while (offset < total_length) {
    // This is just junk left at the end. Break.
    if (total_length - offset < kChunkHeaderSize)
      break;

    // We should be at the beginning of a subsection. The next 8 bytes are the
    // header and should look like: "|f|m|t| |1|2|3|4|" or "|d|a|t|a|1|2|3|4|".
    // Get the |chunk_header| and the |chunk_payload that follows.
    base::StringPiece chunk_header = wav_data.substr(offset, kChunkHeaderSize);
    uint32_t chunk_length = ReadInt<uint32_t>(chunk_header, 4);
    base::StringPiece chunk_payload =
        wav_data.substr(offset + kChunkHeaderSize, chunk_length);

    // Parse the subsection header, handling it if it is a "data" or "fmt "
    // chunk. Skip it otherwise.
    if (chunk_header.starts_with(kFmtSubchunkId)) {
      got_format = true;
      if (!ParseFmtChunk(chunk_payload, params_out))
        return false;
    } else if (chunk_header.starts_with(kDataSubchunkId)) {
      *audio_data_out = chunk_payload;
    } else {
      DVLOG(1) << "Skipping unknown data chunk: " << chunk_header.substr(0, 4)
               << ".";
    }

    offset += kChunkHeaderSize + chunk_length;
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
                                 uint16_t bits_per_sample)
    : data_(audio_data),
      num_channels_(num_channels),
      sample_rate_(sample_rate),
      bits_per_sample_(bits_per_sample) {
  DCHECK_NE(num_channels_, 0u);
  DCHECK_NE(sample_rate_, 0u);
  DCHECK_NE(bits_per_sample_, 0u);
  total_frames_ = data_.size() * 8 / num_channels_ / bits_per_sample_;
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

  return base::WrapUnique(new WavAudioHandler(audio_data, params.num_channels,
                                              params.sample_rate,
                                              params.bits_per_sample));
}

bool WavAudioHandler::AtEnd(size_t cursor) const {
  return data_.size() <= cursor;
}

bool WavAudioHandler::CopyTo(AudioBus* bus,
                             size_t cursor,
                             size_t* bytes_written) const {
  if (!bus)
    return false;
  if (bus->channels() != num_channels_) {
    DVLOG(1) << "Number of channels mismatch.";
    return false;
  }
  if (AtEnd(cursor)) {
    bus->Zero();
    return true;
  }
  const int bytes_per_frame = num_channels_ * bits_per_sample_ / 8;
  const int remaining_frames = (data_.size() - cursor) / bytes_per_frame;
  const int frames = std::min(bus->frames(), remaining_frames);

  bus->FromInterleaved(data_.data() + cursor, frames, bits_per_sample_ / 8);
  *bytes_written = frames * bytes_per_frame;
  bus->ZeroFramesPartial(frames, bus->frames() - frames);
  return true;
}

base::TimeDelta WavAudioHandler::GetDuration() const {
  return base::TimeDelta::FromSecondsD(total_frames_ /
                                       static_cast<double>(sample_rate_));
}

}  // namespace media
