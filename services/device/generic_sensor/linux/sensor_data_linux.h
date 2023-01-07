// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DATA_LINUX_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DATA_LINUX_H_

#include "base/files/file_path.h"
#include "services/device/public/mojom/sensor.mojom.h"

namespace device {

class PlatformSensorConfiguration;
union SensorReading;

// This structure represents a context that is used to identify a udev device
// and create a type specific SensorInfoLinux. For example, when a
// SensorDeviceManager receives a udev device, it uses this structure to
// identify what type of sensor that is and creates a SensorInfoLinux structure
// that holds all the necessary information to create a PlatformSensorLinux.
struct SensorPathsLinux {
  using ReaderFunctor = base::RepeatingCallback<
      void(double scaling, double offset, SensorReading& reading)>;

  SensorPathsLinux();
  ~SensorPathsLinux();
  SensorPathsLinux(const SensorPathsLinux& other);
  // Provides an array of sensor file names to be searched for.
  // Different sensors might have up to 3 different file name arrays.
  // One file must be found from each array.
  std::vector<std::vector<std::string>> sensor_file_names;
  // Scaling file to be found.
  std::string sensor_scale_name;
  // Frequency file to be found.
  std::string sensor_frequency_file_name;
  // Offset file to be found.
  std::string sensor_offset_file_name;
  // Used to apply scalings to raw sensor data.
  ReaderFunctor apply_scaling_func;
  // Sensor type
  mojom::SensorType type;
  // Default configuration of a sensor.
  PlatformSensorConfiguration default_configuration;
};

// Initializes sensor data according to |type|.
bool InitSensorData(mojom::SensorType type, SensorPathsLinux* data);

// This structure represents an iio device, which info is taken
// from udev service. If a client requests a sensor from a provider,
// the provider takes this initialized and stored structure and uses it to
// create a requested PlatformSensorLinux of a certain type.
struct SensorInfoLinux {
  // Represents current sensor device node.
  const std::string device_node;
  // Represents frequency of a sensor.
  const double device_frequency;
  // Represents scaling value to be applied on raw data.
  const double device_scaling_value;
  // Represents offset value that must be applied on raw data.
  const double device_offset_value;
  // Reporting mode of a sensor taken from SensorDataLinux.
  const mojom::ReportingMode reporting_mode;
  // Functor that is used to convert raw data.
  const SensorPathsLinux::ReaderFunctor apply_scaling_func;
  // Sensor files in sysfs. Used to poll data.
  const std::vector<base::FilePath> device_reading_files;

  SensorInfoLinux(const std::string& sensor_device_node,
                  double sensor_device_frequency,
                  double sensor_device_scaling_value,
                  double sensor_device_offset_value,
                  mojom::ReportingMode mode,
                  SensorPathsLinux::ReaderFunctor scaling_func,
                  std::vector<base::FilePath> iio_device_reading_files);
  ~SensorInfoLinux();

  SensorInfoLinux(const SensorInfoLinux&);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_LINUX_SENSOR_DATA_LINUX_H_
