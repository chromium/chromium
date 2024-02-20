// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/orientation_util.h"

#include <cmath>
#include <numbers>

#include "base/check_op.h"
#include "base/numerics/angle_conversions.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"

namespace {

// Returns orientation angles from a rotation matrix, such that the angles are
// according to spec http://dev.w3.org/geo/api/spec-source-orientation.html}.
//
// It is assumed the rotation matrix transforms a 3D column vector from device
// coordinate system to the world's coordinate system.
//
// In particular we compute the decomposition of a given rotation matrix r such
// that
// r = rz(alpha) * rx(beta) * ry(gamma)
// where rz, rx and ry are rotation matrices around z, x and y axes in the world
// coordinate reference frame respectively. The reference frame consists of
// three orthogonal axes x, y, z where x points East, y points north and z
// points upwards perpendicular to the ground plane. The computed angles alpha,
// beta and gamma are in degrees and clockwise-positive when viewed along the
// positive direction of the corresponding axis. Except for the special case
// when the beta angle is +-pi/2 these angles uniquely define the orientation of
// a mobile device in 3D space. The alpha-beta-gamma representation resembles
// the yaw-pitch-roll convention used in vehicle dynamics, however it does not
// exactly match it. One of the differences is that the 'pitch' angle beta is
// allowed to be within [-pi, pi). A mobile device with pitch angle greater than
// pi/2 could correspond to a user lying down and looking upward at the screen.
//
// r is a 9 element rotation matrix:
// r[ 0]   r[ 1]   r[ 2]
// r[ 3]   r[ 4]   r[ 5]
// r[ 6]   r[ 7]   r[ 8]
//
// alpha_in_radians: rotation around the z axis, in [0, 2*pi)
// beta_in_radians: rotation around the x axis, in [-pi, pi)
// gamma_in_radians: rotation around the y axis, in [-pi/2, pi/2)
void ComputeOrientationEulerAnglesInRadiansFromRotationMatrix(
    const std::vector<double>& r,
    double* alpha_in_radians,
    double* beta_in_radians,
    double* gamma_in_radians) {
  DCHECK_EQ(9u, r.size());

  // Since |r| contains double, directly compare it with 0 won't be accurate,
  // so here |device::kEpsilon| is used to check if r[8] and r[6] is close to
  // 0. And this needs to be done before checking if it is greater or less
  // than 0 since a number close to 0 can be either a positive or negative
  // number.
  if (std::abs(r[8]) < device::kEpsilon) {    // r[8] == 0
    if (std::abs(r[6]) < device::kEpsilon) {  // r[6] == 0, cos(beta) == 0
      // gimbal lock discontinuity
      *alpha_in_radians = std::atan2(r[3], r[0]);
      *beta_in_radians = (r[7] > 0) ? std::numbers::pi / 2
                                    : -std::numbers::pi / 2;  // beta = +-pi/2
      *gamma_in_radians = 0;                                 // gamma = 0
    } else if (r[6] > 0) {  // cos(gamma) == 0, cos(beta) > 0
      *alpha_in_radians = std::atan2(-r[1], r[4]);
      *beta_in_radians = std::asin(r[7]);  // beta [-pi/2, pi/2]
      *gamma_in_radians = -std::numbers::pi / 2;  // gamma = -pi/2
    } else {                               // cos(gamma) == 0, cos(beta) < 0
      *alpha_in_radians = std::atan2(r[1], -r[4]);
      *beta_in_radians = -std::asin(r[7]);
      *beta_in_radians +=
          (*beta_in_radians >= 0)
              ? -std::numbers::pi
              : std::numbers::pi;  // beta [-pi,-pi/2) U (pi/2,pi)
      *gamma_in_radians = -std::numbers::pi / 2;  // gamma = -pi/2
    }
  } else if (r[8] > 0) {  // cos(beta) > 0
    *alpha_in_radians = std::atan2(-r[1], r[4]);
    *beta_in_radians = std::asin(r[7]);           // beta (-pi/2, pi/2)
    *gamma_in_radians = std::atan2(-r[6], r[8]);  // gamma (-pi/2, pi/2)
  } else {                                        // cos(beta) < 0
    *alpha_in_radians = std::atan2(r[1], -r[4]);
    *beta_in_radians = -std::asin(r[7]);
    *beta_in_radians += (*beta_in_radians >= 0)
                            ? -std::numbers::pi
                            : std::numbers::pi;  // beta [-pi,-pi/2) U (pi/2,pi)
    *gamma_in_radians = std::atan2(r[6], -r[8]);  // gamma (-pi/2, pi/2)
  }

  // alpha is in [-pi, pi], make sure it is in [0, 2*pi).
  if (*alpha_in_radians < 0)
    *alpha_in_radians += 2 * std::numbers::pi;  // alpha [0, 2*pi)
}

}  // namespace

namespace device {

void ComputeOrientationEulerAnglesFromRotationMatrix(
    const std::vector<double>& r,
    double* alpha_in_degrees,
    double* beta_in_degrees,
    double* gamma_in_degrees) {
  double alpha_in_radians, beta_in_radians, gamma_in_radians;
  ComputeOrientationEulerAnglesInRadiansFromRotationMatrix(
      r, &alpha_in_radians, &beta_in_radians, &gamma_in_radians);
  *alpha_in_degrees = base::RadToDeg(alpha_in_radians);
  *beta_in_degrees = base::RadToDeg(beta_in_radians);
  *gamma_in_degrees = base::RadToDeg(gamma_in_radians);
}

}  // namespace device
