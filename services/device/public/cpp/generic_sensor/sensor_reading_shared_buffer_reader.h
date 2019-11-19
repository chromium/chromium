// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_READER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_READER_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/system/buffer.h"

namespace device {

union SensorReading;
struct SensorReadingSharedBuffer;

class SensorReadingSharedBufferReader {
 public:
  // Creates a new SensorReadingSharedBufferReader instance that reads
  // sensor readings from the shared buffer.
  static std::unique_ptr<SensorReadingSharedBufferReader> Create(
      mojo::ScopedSharedBufferHandle reading_buffer_handle,
      uint64_t reading_buffer_offset);

  ~SensorReadingSharedBufferReader();

  // Get sensor reading from shared buffer.
  bool GetReading(SensorReading* result);
  static bool GetReading(const SensorReadingSharedBuffer* buffer,
                         SensorReading* result);

 private:
  explicit SensorReadingSharedBufferReader(
      mojo::ScopedSharedBufferHandle shared_buffer_handle,
      mojo::ScopedSharedBufferMapping shared_buffer);

  static bool TryReadFromBuffer(const SensorReadingSharedBuffer* buffer,
                                SensorReading* result);

  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  mojo::ScopedSharedBufferMapping shared_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorReadingSharedBufferReader);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_READER_H_
