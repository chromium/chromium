// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

class PlatformSensorAndroid : public PlatformSensor {
 public:
  // Creates a new PlatformSensorAndroid for the given sensor type, returning
  // nullptr if it is not supported by the platform.
  static scoped_refptr<PlatformSensorAndroid> Create(
      mojom::SensorType type,
      SensorReadingSharedBuffer* reading_buffer,
      base::WeakPtr<PlatformSensorProvider> provider,
      const base::android::JavaRef<jobject>& java_provider);

  PlatformSensorAndroid(mojom::SensorType type,
                        SensorReadingSharedBuffer* reading_buffer,
                        base::WeakPtr<PlatformSensorProvider> provider);

  PlatformSensorAndroid(const PlatformSensorAndroid&) = delete;
  PlatformSensorAndroid& operator=(const PlatformSensorAndroid&) = delete;

  mojom::ReportingMode GetReportingMode() override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;
  double GetMaximumSupportedFrequency() override;

  void NotifyPlatformSensorError(JNIEnv*,
                                 const base::android::JavaRef<jobject>& caller);

  void UpdatePlatformSensorReading(
      JNIEnv*,
      const base::android::JavaRef<jobject>& caller,
      jdouble timestamp,
      jdouble value1,
      jdouble value2,
      jdouble value3,
      jdouble value4);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObjectForTesting() {
    return j_object_;
  }

  // Simulate a `SensorEvent` from
  // android.hardware.Sensor. The simulated event is created
  // with length of |reading_values_length| and filled with readings with
  // (reading_index + 0.1).
  static void SimulateSensorEventFromJavaForTesting(
      base::android::ScopedJavaGlobalRef<jobject> j_object_,
      jint reading_values_length);

 protected:
  ~PlatformSensorAndroid() override;
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;

 private:
  // Java object org.chromium.device.sensors.PlatformSensor
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_
