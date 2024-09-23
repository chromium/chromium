// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_READER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_READER_H_

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"

namespace device {

union SensorReading;
template <class T>
struct SensorReadingSharedBufferImpl;
using SensorReadingSharedBuffer = SensorReadingSharedBufferImpl<void>;

class SensorReadingSharedBufferReader {
 public:
  // Creates a new SensorReadingSharedBufferReader instance that reads
  // sensor readings from the shared buffer.
  static std::unique_ptr<SensorReadingSharedBufferReader> Create(
      base::ReadOnlySharedMemoryRegion region,
      uint64_t reading_buffer_offset);

  SensorReadingSharedBufferReader(const SensorReadingSharedBufferReader&) =
      delete;
  SensorReadingSharedBufferReader& operator=(
      const SensorReadingSharedBufferReader&) = delete;

  ~SensorReadingSharedBufferReader();

  // Get sensor reading from shared buffer.
  bool GetReading(SensorReading* result);
  static bool GetReading(const SensorReadingSharedBuffer* buffer,
                         SensorReading* result);

 private:
  explicit SensorReadingSharedBufferReader(
      base::ReadOnlySharedMemoryMapping mapping);

  base::ReadOnlySharedMemoryMapping mapping_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_READER_H_
