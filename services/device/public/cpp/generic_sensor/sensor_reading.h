// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_H_

#include <stddef.h>
#include <stdint.h>

#include <type_traits>

namespace device {

// This class is guaranteed to have a fixed size of 64 bits on every platform.
// It is introduce to simplify sensors shared buffer memory calculation.
template <typename Data>
class SensorReadingField {
 public:
  static_assert(sizeof(Data) <= sizeof(int64_t),
                "The field size must be <= 64 bits.");
  static_assert(std::is_trivially_destructible<Data>::value,
                "Data must be a trivially destructible type.");

  SensorReadingField() = default;
  SensorReadingField(Data value) { storage_.value = value; }
  SensorReadingField& operator=(Data value) {
    storage_.value = value;
    return *this;
  }
  Data& value() { return storage_.value; }
  const Data& value() const { return storage_.value; }

  operator Data() const { return storage_.value; }

 private:
  union Storage {
    int64_t unused;
    Data value;
    Storage() {
      // There is a static_assert above that checks that Data is trivially
      // destructible, so we do not need a custom destructor here that invokes
      // Data's and can keep Storage and SensorReadingField trivially copyable.
      new (&value) Data();
    }
  };
  Storage storage_;
};

struct SensorReadingBase {
  SensorReadingBase();
  ~SensorReadingBase() = default;
  SensorReadingField<double> timestamp;
};

// Represents raw sensor reading data: timestamp and 4 values.
struct SensorReadingRaw : public SensorReadingBase {
  SensorReadingRaw();
  ~SensorReadingRaw() = default;

  constexpr static size_t kValuesCount = 4;
  SensorReadingField<double> values[kValuesCount];
};

// Represents a single data value.
struct SensorReadingSingle : public SensorReadingBase {
  SensorReadingSingle();
  ~SensorReadingSingle() = default;
  SensorReadingField<double> value;
};

// Represents a vector in 3d coordinate system.
struct SensorReadingXYZ : public SensorReadingBase {
  SensorReadingXYZ();
  ~SensorReadingXYZ() = default;
  SensorReadingField<double> x;
  SensorReadingField<double> y;
  SensorReadingField<double> z;
};

// Represents quaternion.
struct SensorReadingQuat : public SensorReadingXYZ {
  SensorReadingQuat();
  ~SensorReadingQuat() = default;
  SensorReadingField<double> w;
};

// A common type to represent sensor reading.
// For every implemented sensor type, the reading is stored as described below:
//
// AMBIENT_LIGHT:
// als.value: ambient light level in SI lux units.
//
// ACCELEROMETER:
// accel.x: acceleration minus Gx on the x-axis in SI meters per second
// squared (m/s^2) units. It corresponds to Accelerometer.x in the W3C
// Accelerometer Specification.
// accel.y: acceleration minus Gy on the y-axis in SI meters per second
// squared (m/s^2) units. It corresponds to Accelerometer.y in the W3C
// Accelerometer Specification.
// accel.z: acceleration minus Gz on the z-axis in SI meters per second
// squared (m/s^2) units. It corresponds to Accelerometer.y in the W3C
// Accelerometer Specification.
//
// LINEAR_ACCELERATION:
// accel.x: acceleration on the x-axis in SI meters per second squared
// (m/s^2) units. It corresponds to LinearAccelerationSensor.x in the W3C
// Accelerometer Specification.
// accel.y: acceleration on the y-axis in SI meters per second squared
// (m/s^2) units. It corresponds to LinearAccelerationSensor.y in the W3C
// Accelerometer Specification.
// accel.z: acceleration on the z-axis in SI meters per second squared
// (m/s^2) units. It corresponds to LinearAccelerationSensor.z in the W3C
// Accelerometer Specification.
//
// GRAVITY:
// accel.x: acceleration on the x-axis in SI meters per second squared
// (m/s^2) units. It corresponds to GravitySensor.x in the W3C
// Accelerometer Specification.
// accel.y: acceleration on the y-axis in SI meters per second squared
// (m/s^2) units. It corresponds to GravitySensor.y in the W3C
// Accelerometer Specification.
// accel.z: acceleration on the z-axis in SI meters per second squared
// (m/s^2) units. It corresponds to GravitySensor.z in the W3C
// Accelerometer Specification.
//
// GYROSCOPE:
// gyro.x: angular speed around the x-axis in radians/second. It corresponds to
// Gyroscope.x in the W3C Gyroscope Specification.
// gyro.y: angular speed around the y-axis in radians/second. It corresponds to
// Gyroscope.y in the W3C Gyroscope Specification.
// gyro.z: angular speed around the z-axis in radians/second. It corresponds to
// Gyroscope.z in the W3C Gyroscope Specification.
//
// MAGNETOMETER:
// magn.x: ambient magnetic field in the x-axis in micro-Tesla (uT).
// magn.y: ambient magnetic field in the y-axis in micro-Tesla (uT).
// magn.z: ambient magnetic field in the z-axis in micro-Tesla (uT).
//
// ABSOLUTE_ORIENTATION_EULER_ANGLES:
// orientation_euler.x: x-axis angle in degrees representing the orientation of
// the device in 3D space. It corresponds to the beta value in the W3C
// DeviceOrientation Event Specification. This value is in [-180, 180).
// orientation_euler.y: y-axis angle in degrees representing the orientation of
// the device in 3D space. It corresponds to the gamma value in the W3C
// DeviceOrientation Event Specification. This value is in [-90, 90).
// orientation_euler.z: z-axis angle in degrees representing the orientation of
// the device in 3D space. It corresponds to the alpha value in the W3C
// DeviceOrientation Event Specification. This value is in [0, 360).
//
// ABSOLUTE_ORIENTATION_QUATERNION:
// orientation_quat.x: x value of a quaternion representing the orientation of
// the device in 3D space.
// orientation_quat.y: y value of a quaternion representing the orientation of
// the device in 3D space.
// orientation_quat.z: z value of a quaternion representing the orientation of
// the device in 3D space.
// orientation_quat.w: w value of a quaternion representing the orientation of
// the device in 3D space.
//
// RELATIVE_ORIENTATION_EULER_ANGLES:
// (Identical to ABSOLUTE_ORIENTATION_EULER_ANGLES except that it doesn't use
// the geomagnetic field.)
// orientation_euler.x: x-axis angle in degrees representing the orientation of
// the device in 3D space. It corresponds to the beta value in the W3C
// DeviceOrientation Event Specification. This value is in [-180, 180).
// orientation_euler.y: y-axis angle in degrees representing the orientation of
// the device in 3D space. It corresponds to the gamma value in the W3C
// DeviceOrientation Event Specification. This value is in [-90, 90).
// orientation_euler.z: z-axis angle in degrees representing the orientation of
// the device in 3D space. It corresponds to the alpha value in the W3C
// DeviceOrientation Event Specification. This value is in [0, 360).
//
// RELATIVE_ORIENTATION_QUATERNION:
// (Identical to ABSOLUTE_ORIENTATION_QUATERNION except that it doesn't use
// the geomagnetic field.)
// orientation_quat.x: x value of a quaternion representing the orientation of
// the device in 3D space.
// orientation_quat.y: y value of a quaternionrepresenting the orientation of
// the device in 3D space.
// orientation_quat.z: z value of a quaternion representing the orientation of
// the device in 3D space.
// orientation_quat.w: w value of a quaternion representing the orientation of
// the device in 3D space.

union SensorReading {
  static_assert(std::is_trivially_destructible<SensorReadingRaw>::value,
                "SensorReading's fields must be trivially destructible.");

  SensorReadingRaw raw;
  SensorReadingSingle als;             // AMBIENT_LIGHT
  SensorReadingXYZ accel;  // ACCELEROMETER, LINEAR_ACCELERATION, GRAVITY
  SensorReadingXYZ gyro;               // GYROSCOPE
  SensorReadingXYZ magn;               // MAGNETOMETER
  SensorReadingQuat orientation_quat;  // ABSOLUTE_ORIENTATION_QUATERNION,
                                       // RELATIVE_ORIENTATION_QUATERNION
  SensorReadingXYZ orientation_euler;  // ABSOLUTE_ORIENTATION_EULER_ANGLES,
                                       // RELATIVE_ORIENTATION_EULER_ANGLES

  double timestamp() const { return raw.timestamp; }

  SensorReading();
  ~SensorReading() = default;
};

static_assert(sizeof(SensorReading) == sizeof(SensorReadingRaw),
              "Check SensorReading size.");
static_assert(std::is_trivially_copyable<SensorReading>::value,
              "SensorReading must be trivially copyable.");

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_SENSOR_READING_H_
