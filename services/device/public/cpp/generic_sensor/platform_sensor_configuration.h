// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_


namespace device {

class PlatformSensorConfiguration {
 public:
  PlatformSensorConfiguration();
  explicit PlatformSensorConfiguration(double frequency);
  ~PlatformSensorConfiguration();

  bool operator==(const PlatformSensorConfiguration& other) const;

  // Only frequency is used to compare two configurations.
  bool operator>(const PlatformSensorConfiguration& other) const;

  void set_frequency(double frequency);
  double frequency() const { return frequency_; }

 private:
  double frequency_ = 1.0;  // 1 Hz by default.
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_
