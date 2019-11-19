// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_PLATFORM_SENSOR_AMBIENT_LIGHT_SENSOR_MAC_H_
#define DEVICE_GENERIC_PLATFORM_SENSOR_AMBIENT_LIGHT_SENSOR_MAC_H_

#include <IOKit/IOKitLib.h>

#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

// Implementation of PlatformSensor for macOS to query the ambient light sensor.
// This is a single instance object per browser process which is created by
// PlatformSensorProviderMac. If there are no clients, this instance is not
// created.
class PlatformSensorAmbientLightMac : public PlatformSensor {
 public:
  // Construct a platform sensor of AMBIENT_LIGHT, given a buffer |mapping|
  // to write the result back.
  PlatformSensorAmbientLightMac(SensorReadingSharedBuffer* reading_buffer,
                                PlatformSensorProvider* provider);

  mojom::ReportingMode GetReportingMode() override;
  // Can only be called once, the first time or after a StopSensor call.
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;

 protected:
  ~PlatformSensorAmbientLightMac() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;

 private:
  bool ReadAndUpdate();
  static void IOServiceCallback(void* context,
                                io_service_t service,
                                natural_t message_type,
                                void* message_argument);

  // IOService representing the LMU sensor.
  base::mac::ScopedIOObject<io_service_t> light_sensor_service_;
  // Port used to get the notifications from the sensor.
  base::mac::ScopedIONotificationPortRef light_sensor_port_;
  // IO Object used to query the value of the sensor.
  base::mac::ScopedIOObject<io_object_t> light_sensor_object_;
  // IO Notifications created by IOServiceAddInterestNotification.
  base::mac::ScopedIOObject<io_object_t> light_sensor_notification_;
  // IO Notifications created by IOServiceAddInterestNotification when the
  // sensor is busy.
  base::mac::ScopedIOObject<io_object_t> light_sensor_busy_notification_;
  double current_lux_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorAmbientLightMac);
};

}  // namespace device

#endif  // DEVICE_GENERIC_PLATFORM_SENSOR_AMBIENT_LIGHT_SENSOR_MAC_H_
