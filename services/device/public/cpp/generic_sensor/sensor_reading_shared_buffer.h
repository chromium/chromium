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
//
// TODO(crbug.com/355003174): It's a template to avoid the clang plugin that
// prevents inline ctors, as we need the class to be trivially copyable for use
// in shared memory.
template <class T = void>
struct SensorReadingSharedBufferImpl {
  SensorReadingField<OneWriterSeqLock> seqlock;
  SensorReading reading;
};

using SensorReadingSharedBuffer = SensorReadingSharedBufferImpl<void>;

// Gets the shared reading buffer offset for the given sensor type.
uint64_t GetSensorReadingSharedBufferOffset(mojom::SensorType type);

// SensorReadingSharedBuffer is used in shared memory, so it must be trivially
// copyable.
static_assert(std::is_trivially_copyable_v<SensorReadingSharedBuffer>);

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_SHARED_BUFFER_H_
