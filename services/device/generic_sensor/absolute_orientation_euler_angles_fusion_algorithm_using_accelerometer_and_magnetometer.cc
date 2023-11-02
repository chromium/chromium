// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"

#include <cmath>

#include "base/check.h"
#include "base/numerics/math_constants.h"
#include "services/device/generic_sensor/orientation_util.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"

namespace {

// Helper function to compute the rotation matrix using gravity and geomagnetic
// data. Returns a 9 element rotation matrix:
// r[ 0]   r[ 1]   r[ 2]
// r[ 3]   r[ 4]   r[ 5]
// r[ 6]   r[ 7]   r[ 8]
// r is meaningful only when the device is not free-falling and it is not close
// to the magnetic north. If the device is accelerating, or placed into a
// strong magnetic field, the returned matricx may be inaccurate.
//
// Free fall is defined as condition when the magnitude of the gravity is less
// than 1/10 of the nominal value.
bool ComputeRotationMatrixFromGravityAndGeomagnetic(double gravity_x,
                                                    double gravity_y,
                                                    double gravity_z,
                                                    double geomagnetic_x,
                                                    double geomagnetic_y,
                                                    double geomagnetic_z,
                                                    std::vector<double>* r) {
  double gravity_squared =
      (gravity_x * gravity_x + gravity_y * gravity_y + gravity_z * gravity_z);
  double free_fall_gravity_squared =
      0.01 * base::kMeanGravityDouble * base::kMeanGravityDouble;
  if (gravity_squared < free_fall_gravity_squared) {
    // gravity less than 10% of normal value
    return false;
  }

  double hx = geomagnetic_y * gravity_z - geomagnetic_z * gravity_y;
  double hy = geomagnetic_z * gravity_x - geomagnetic_x * gravity_z;
  double hz = geomagnetic_x * gravity_y - geomagnetic_y * gravity_x;
  double norm_h = std::sqrt(hx * hx + hy * hy + hz * hz);
  if (norm_h < 0.1) {
    // device is close to free fall, or close to magnetic north pole.
    // Typical values are  > 100.
    return false;
  }

  double inv_h = 1.0 / norm_h;
  hx *= inv_h;
  hy *= inv_h;
  hz *= inv_h;
  double inv_gravity = 1.0 / std::sqrt(gravity_squared);
  gravity_x *= inv_gravity;
  gravity_y *= inv_gravity;
  gravity_z *= inv_gravity;
  double mx = gravity_y * hz - gravity_z * hy;
  double my = gravity_z * hx - gravity_x * hz;
  double mz = gravity_x * hy - gravity_y * hx;

  r->resize(9);

  (*r)[0] = hx;
  (*r)[1] = hy;
  (*r)[2] = hz;
  (*r)[3] = mx;
  (*r)[4] = my;
  (*r)[5] = mz;
  (*r)[6] = gravity_x;
  (*r)[7] = gravity_y;
  (*r)[8] = gravity_z;

  return true;
}

bool ComputeAbsoluteOrientationEulerAnglesFromGravityAndGeomagnetic(
    double gravity_x,
    double gravity_y,
    double gravity_z,
    double geomagnetic_x,
    double geomagnetic_y,
    double geomagnetic_z,
    double* alpha_in_degrees,
    double* beta_in_degrees,
    double* gamma_in_degrees) {
  std::vector<double> rotation_matrix;
  if (!ComputeRotationMatrixFromGravityAndGeomagnetic(
          gravity_x, gravity_y, gravity_z, geomagnetic_x, geomagnetic_y,
          geomagnetic_z, &rotation_matrix)) {
    return false;
  }

  device::ComputeOrientationEulerAnglesFromRotationMatrix(
      rotation_matrix, alpha_in_degrees, beta_in_degrees, gamma_in_degrees);
  return true;
}

}  // namespace

namespace device {

AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer::
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer()
    : PlatformSensorFusionAlgorithm(
          mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
          {mojom::SensorType::ACCELEROMETER, mojom::SensorType::MAGNETOMETER}) {
}

AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer::
    ~AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer() =
        default;

bool AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer::
    GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                         SensorReading* fused_reading) {
  // Transform the accelerometer and magnetometer values to W3C draft Euler
  // angles.
  DCHECK(fusion_sensor_);

  // Only generate a new sensor value when the accelerometer reading changes.
  if (which_sensor_changed != mojom::SensorType::ACCELEROMETER)
    return false;

  SensorReading gravity_reading;
  SensorReading geomagnetic_reading;
  if (!fusion_sensor_->GetSourceReading(mojom::SensorType::ACCELEROMETER,
                                        &gravity_reading) ||
      !fusion_sensor_->GetSourceReading(mojom::SensorType::MAGNETOMETER,
                                        &geomagnetic_reading)) {
    return false;
  }

  return ComputeAbsoluteOrientationEulerAnglesFromGravityAndGeomagnetic(
      gravity_reading.accel.x, gravity_reading.accel.y, gravity_reading.accel.z,
      geomagnetic_reading.magn.x, geomagnetic_reading.magn.y,
      geomagnetic_reading.magn.z,
      &fused_reading->orientation_euler.z.value() /* alpha_in_degrees */,
      &fused_reading->orientation_euler.x.value() /* beta_in_degrees */,
      &fused_reading->orientation_euler.y.value() /* gamma_in_degrees */);
}

}  // namespace device
