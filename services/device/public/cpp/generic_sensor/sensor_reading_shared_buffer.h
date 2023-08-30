// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_H_

#include "device/base/synchronization/one_writer_seqlock.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom-shared.h"

namespace device {

// This structure represents sensor reading buffer: sensor reading and seqlock
// for synchronization.
struct SensorReadingSharedBuffer {
  SensorReadingSharedBuffer();
  ~SensorReadingSharedBuffer();
  SensorReadingField<OneWriterSeqLock> seqlock;
  SensorReading reading;

  // Gets the shared reading buffer offset for the given sensor type.
  static uint64_t GetOffset(mojom::SensorType type);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_H_
