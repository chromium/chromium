// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

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

  // TODO(someone): This *should* use GetMemoryAs, but SensorReadingSharedBuffer
  // is not considered trivially copyable. Maybe there's a better trait to
  // use...
  const auto* buffer =
      static_cast<const device::SensorReadingSharedBuffer*>(mapping_.memory());

  return GetReading(buffer, result);
}

// static
bool SensorReadingSharedBufferReader::GetReading(
    const SensorReadingSharedBuffer* buffer,
    SensorReading* result) {
  DCHECK(buffer);

  int read_attempts = 0;
  while (!TryReadFromBuffer(buffer, result)) {
    // Only try to read this many times before failing to avoid waiting here
    // very long in case of contention with the writer.
    if (++read_attempts == kMaxReadAttemptsCount) {
      // Failed to successfully read, presumably because the hardware
      // thread was taking unusually long. Data in |result| was not updated
      // and was simply left what was there before.
      return false;
    }
  }

  return true;
}

// static
bool SensorReadingSharedBufferReader::TryReadFromBuffer(
    const SensorReadingSharedBuffer* buffer,
    SensorReading* result) {
  DCHECK(buffer);

  auto version = buffer->seqlock.value().ReadBegin();
  SensorReading temp_reading_data = buffer->reading;
  if (buffer->seqlock.value().ReadRetry(version))
    return false;
  *result = temp_reading_data;
  return true;
}

}  // namespace device
