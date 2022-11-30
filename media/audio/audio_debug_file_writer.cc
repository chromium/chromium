// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_file_writer.h"

#include <stdint.h>
#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/sys_byteorder.h"
#include "base/task/sequenced_task_runner.h"
#include "media/audio/audio_bus_pool.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"

namespace media {

namespace {

// Windows WAVE format header
// Byte order: Little-endian
// Offset Length  Content
//  0      4     "RIFF"
//  4      4     <file length - 8>
//  8      4     "WAVE"
// 12      4     "fmt "
// 16      4     <length of the fmt data> (=16)
// 20      2     <WAVE file encoding tag>
// 22      2     <channels>
// 24      4     <sample rate>
// 28      4     <bytes per second> (sample rate * block align)
// 32      2     <block align>  (channels * bits per sample / 8)
// 34      2     <bits per sample>
// 36      4     "data"
// 40      4     <sample data size(n)>
// 44     (n)    <sample data>

// We write 16 bit PCM only.
static const uint16_t kBytesPerSample = 2;

static const uint32_t kWavHeaderSize = 44;
static const uint32_t kFmtChunkSize = 16;
// 4 bytes for ID + 4 bytes for size.
static const uint32_t kChunkHeaderSize = 8;
static const uint16_t kWavFormatPcm = 1;

static const char kRiff[] = {'R', 'I', 'F', 'F'};
static const char kWave[] = {'W', 'A', 'V', 'E'};
static const char kFmt[] = {'f', 'm', 't', ' '};
static const char kData[] = {'d', 'a', 't', 'a'};

typedef std::array<char, kWavHeaderSize> WavHeaderBuffer;

class CharBufferWriter {
 public:
  CharBufferWriter(char* buf, int max_size)
      : buf_(buf), max_size_(max_size), size_(0) {}

  CharBufferWriter(const CharBufferWriter&) = delete;
  CharBufferWriter& operator=(const CharBufferWriter&) = delete;

  void Write(const char* data, int data_size) {
    CHECK_LE(size_ + data_size, max_size_);
    memcpy(&buf_[size_], data, data_size);
    size_ += data_size;
  }

  void Write(const char (&data)[4]) {
    Write(static_cast<const char*>(data), 4);
  }

  void WriteLE16(uint16_t data) {
    uint16_t val = base::ByteSwapToLE16(data);
    Write(reinterpret_cast<const char*>(&val), sizeof(val));
  }

  void WriteLE32(uint32_t data) {
    uint32_t val = base::ByteSwapToLE32(data);
    Write(reinterpret_cast<const char*>(&val), sizeof(val));
  }

 private:
  raw_ptr<char> buf_;
  const int max_size_;
  int size_;
};

// Writes Wave header to the specified address, there should be at least
// kWavHeaderSize bytes allocated for it.
void WriteWavHeader(WavHeaderBuffer* buf,
                    uint32_t channels,
                    uint32_t sample_rate,
                    uint64_t samples) {
  // We'll need to add (kWavHeaderSize - kChunkHeaderSize) to payload to
  // calculate Riff chunk size.
  static const uint32_t kMaxBytesInPayload =
      std::numeric_limits<uint32_t>::max() -
      (kWavHeaderSize - kChunkHeaderSize);
  const uint64_t bytes_in_payload_64 = samples * kBytesPerSample;

  // In case payload is too large and causes uint32_t overflow, we just specify
  // the maximum possible value; all the payload above that count will be
  // interpreted as garbage.
  const uint32_t bytes_in_payload = bytes_in_payload_64 > kMaxBytesInPayload
                                        ? kMaxBytesInPayload
                                        : bytes_in_payload_64;
  LOG_IF(WARNING, bytes_in_payload < bytes_in_payload_64)
      << "Number of samples is too large and will be clipped by Wave header,"
      << " all the data above " << kMaxBytesInPayload
      << " bytes will appear as junk";
  const uint32_t block_align = channels * kBytesPerSample;
  const uint32_t byte_rate = channels * sample_rate * kBytesPerSample;
  const uint32_t riff_chunk_size =
      bytes_in_payload + kWavHeaderSize - kChunkHeaderSize;

  CharBufferWriter writer(&(*buf)[0], kWavHeaderSize);

  writer.Write(kRiff);
  writer.WriteLE32(riff_chunk_size);
  writer.Write(kWave);
  writer.Write(kFmt);
  writer.WriteLE32(kFmtChunkSize);
  writer.WriteLE16(kWavFormatPcm);
  writer.WriteLE16(channels);
  writer.WriteLE32(sample_rate);
  writer.WriteLE32(byte_rate);
  writer.WriteLE16(block_align);
  writer.WriteLE16(kBytesPerSample * 8);
  writer.Write(kData);
  writer.WriteLE32(bytes_in_payload);
}

}  // namespace

AudioDebugFileWriter::~AudioDebugFileWriter() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (file_.IsValid())
    WriteHeader();
}

void AudioDebugFileWriter::Write(const AudioBus& data) {
  std::unique_ptr<AudioBus> data_copy = audio_bus_pool_->GetAudioBus();
  DCHECK(data_copy);
  data.CopyTo(data_copy.get());
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioDebugFileWriter::DoWrite, weak_this_,
                                std::move(data_copy)));
}

AudioDebugFileWriter::Ptr AudioDebugFileWriter::Create(
    const AudioParameters& params,
    base::File file) {
  return Create(params, std::move(file),
                std::make_unique<AudioBusPoolImpl>(
                    params, kPreallocatedAudioBuses, kMaxCachedAudioBuses));
}

AudioDebugFileWriter::AudioDebugFileWriter(
    const AudioParameters& params,
    base::File file,
    std::unique_ptr<AudioBusPool> audio_bus_pool)
    : params_(params),
      file_(std::move(file)),
      audio_bus_pool_(std::move(audio_bus_pool)) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

AudioDebugFileWriter::Ptr AudioDebugFileWriter::Create(
    const AudioParameters& params,
    base::File file,
    std::unique_ptr<AudioBusPool> audio_bus_pool) {
  AudioDebugFileWriter* writer = new AudioDebugFileWriter(
      params, std::move(file), std::move(audio_bus_pool));
  writer->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioDebugFileWriter::WriteHeader, writer->weak_this_));
  return Ptr(writer, base::OnTaskRunnerDeleter(writer->task_runner_));
}

void AudioDebugFileWriter::DoWrite(std::unique_ptr<AudioBus> data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(params_.channels(), data->channels());
  if (!file_.IsValid())
    return;

  // Convert to 16 bit audio and write to file.
  int data_size = data->frames() * data->channels();
  if (!interleaved_data_ || interleaved_data_size_ < data_size) {
    interleaved_data_.reset(new int16_t[data_size]);
    interleaved_data_size_ = data_size;
  }
  samples_ += data_size;
  data->ToInterleaved<media::SignedInt16SampleTypeTraits>(
      data->frames(), interleaved_data_.get());

#ifndef ARCH_CPU_LITTLE_ENDIAN
  static_assert(sizeof(interleaved_data_[0]) == sizeof(uint16_t),
                "Only 2 bytes per channel is supported.");
  for (int i = 0; i < data_size; ++i)
    interleaved_data_[i] = base::ByteSwapToLE16(interleaved_data_[i]);
#endif

  file_.WriteAtCurrentPos(reinterpret_cast<char*>(interleaved_data_.get()),
                          data_size * sizeof(interleaved_data_[0]));

  // Cache the AudioBus for later use.
  audio_bus_pool_->InsertAudioBus(std::move(data));
}

void AudioDebugFileWriter::WriteHeader() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!file_.IsValid())
    return;
  WavHeaderBuffer buf;
  WriteWavHeader(&buf, params_.channels(), params_.sample_rate(), samples_);
  file_.Write(0, &buf[0], kWavHeaderSize);

  // Write() does not move the cursor if file is not in APPEND mode; Seek() so
  // that the header is not overwritten by the following writes.
  file_.Seek(base::File::FROM_BEGIN, kWavHeaderSize);
}

}  // namespace media
