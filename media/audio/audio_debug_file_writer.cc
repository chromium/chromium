// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/audio_debug_file_writer.h"

#include <stdint.h>
#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_bus_pool.h"
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

static const uint8_t kRiff[] = {'R', 'I', 'F', 'F'};
static const uint8_t kWave[] = {'W', 'A', 'V', 'E'};
static const uint8_t kFmt[] = {'f', 'm', 't', ' '};
static const uint8_t kData[] = {'d', 'a', 't', 'a'};

using WavHeaderBuffer = std::array<char, kWavHeaderSize>;

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

  base::SpanWriter writer(
      base::as_writable_bytes(base::span(*buf).first(kWavHeaderSize)));

  writer.Write(kRiff);
  writer.WriteU32LittleEndian(riff_chunk_size);
  writer.Write(kWave);
  writer.Write(kFmt);
  writer.WriteU32LittleEndian(kFmtChunkSize);
  writer.WriteU16LittleEndian(kWavFormatPcm);
  writer.WriteU16LittleEndian(channels);
  writer.WriteU32LittleEndian(sample_rate);
  writer.WriteU32LittleEndian(byte_rate);
  writer.WriteU16LittleEndian(block_align);
  writer.WriteU16LittleEndian(kBytesPerSample * 8);
  writer.Write(kData);
  writer.WriteU32LittleEndian(bytes_in_payload);
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
  auto data_size =
      base::checked_cast<size_t>(data->frames() * data->channels());
  if (!interleaved_data_ || interleaved_data_->size() < data_size) {
    // This buffer will be initialized fully by the ToInterleaved() call below.
    interleaved_data_.emplace(base::HeapArray<int16_t>::Uninit(data_size));
  }
  data->ToInterleaved<media::SignedInt16SampleTypeTraits>(
      data->frames(), interleaved_data_->data());
  samples_ += data_size;

  // `interleaved_data_` is in little endian format, which is what we want
  // to write to the file.
  static_assert(ARCH_CPU_LITTLE_ENDIAN);

  file_.WriteAtCurrentPos(base::as_bytes(interleaved_data_->as_span()));

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
