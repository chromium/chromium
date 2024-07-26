// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"

namespace {

constexpr int kMaxReadAttemptsCount = 10;

}  // namespace

namespace device {

SensorReadingSharedBufferReader::SensorReadingSharedBufferReader(
    base::ReadOnlySharedMemoryMapping mapping)
    : mapping_(std::move(mapping)) {}

SensorReadingSharedBufferReader::~SensorReadingSharedBufferReader() = default;

// static
std::unique_ptr<SensorReadingSharedBufferReader>
SensorReadingSharedBufferReader::Create(base::ReadOnlySharedMemoryRegion region,
                                        uint64_t reading_buffer_offset) {
  constexpr size_t kReadBufferSize = sizeof(SensorReadingSharedBuffer);
  DCHECK_EQ(0u, reading_buffer_offset % kReadBufferSize);

  base::ReadOnlySharedMemoryMapping mapping =
      region.MapAt(reading_buffer_offset, kReadBufferSize);

  if (!mapping.IsValid())
    return nullptr;

  return base::WrapUnique(
      new SensorReadingSharedBufferReader(std::move(mapping)));
}

bool SensorReadingSharedBufferReader::GetReading(SensorReading* result) {
  DCHECK(mapping_.IsValid());
  return GetReading(mapping_.GetMemoryAs<device::SensorReadingSharedBuffer>(),
                    result);
}

// static
bool SensorReadingSharedBufferReader::GetReading(
    const SensorReadingSharedBuffer* buffer,
    SensorReading* result) {
  DCHECK(buffer);

  uint32_t retries = 0;
  int32_t version;
  do {
    version = buffer->seqlock.value().ReadBegin();
    device::OneWriterSeqLock::AtomicReaderMemcpy(result, &buffer->reading,
                                                 sizeof(SensorReading));
  } while (buffer->seqlock.value().ReadRetry(version) &&
           ++retries < kMaxReadAttemptsCount);

  // Consider the number of retries less than kMaxRetries as success.
  return retries < kMaxReadAttemptsCount;
}

}  // namespace device
