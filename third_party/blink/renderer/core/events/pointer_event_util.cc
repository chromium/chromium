// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/pointer_event_util.h"

#include <cmath>

#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {
// static
double PointerEventUtil::AzimuthFromTilt(double tilt_x_degrees,
                                         double tilt_y_degrees) {
  DCHECK(tilt_x_degrees >= -90 && tilt_x_degrees <= 90);
  DCHECK(tilt_y_degrees >= -90 && tilt_y_degrees <= 90);

  if (tilt_x_degrees == 0) {
    if (tilt_y_degrees > 0) {
      return kPiOverTwoDouble;
    }
    if (tilt_y_degrees < 0) {
      return 3.0 * kPiOverTwoDouble;
    }
    return 0.0;
  }

  if (tilt_y_degrees == 0) {
    if (tilt_x_degrees < 0) {
      return kPiDouble;
    }
    return 0.0;
  }

  if (abs(tilt_x_degrees) == 90 || abs(tilt_y_degrees) == 90) {
    return 0.0;
  }

  DCHECK(tilt_x_degrees != 0.0 && tilt_y_degrees != 0.0 &&
         abs(tilt_x_degrees) != 90 && abs(tilt_y_degrees) != 90);
  const double tilt_x_radians = kPiDouble / 180.0 * tilt_x_degrees;
  const double tilt_y_radians = kPiDouble / 180.0 * tilt_y_degrees;
  const double tan_x = tan(tilt_x_radians);
  const double tan_y = tan(tilt_y_radians);
  double azimuth_radians = atan2(tan_y, tan_x);
  azimuth_radians = (azimuth_radians >= 0) ? azimuth_radians
                                           : (azimuth_radians + kTwoPiDouble);

  DCHECK(azimuth_radians >= 0 && azimuth_radians <= kTwoPiDouble);
  return azimuth_radians;
}

// static
double PointerEventUtil::AltitudeFromTilt(double tilt_x_degrees,
                                          double tilt_y_degrees) {
  DCHECK(tilt_x_degrees >= -90 && tilt_x_degrees <= 90);
  DCHECK(tilt_y_degrees >= -90 && tilt_y_degrees <= 90);

  const double tilt_x_radians = kPiDouble / 180.0 * tilt_x_degrees;
  const double tilt_y_radians = kPiDouble / 180.0 * tilt_y_degrees;

  if (abs(tilt_x_degrees) == 90 || abs(tilt_y_degrees) == 90) {
    return 0;
  }
  if (tilt_x_degrees == 0) {
    return kPiOverTwoDouble - abs(tilt_y_radians);
  }
  if (tilt_y_degrees == 0) {
    return kPiOverTwoDouble - abs(tilt_x_radians);
  }

  return atan(1.0 /
              sqrt(pow(tan(tilt_x_radians), 2) + pow(tan(tilt_y_radians), 2)));
}

// static
int32_t PointerEventUtil::TiltXFromSpherical(double azimuth_radians,
                                             double altitude_radians) {
  DCHECK(azimuth_radians >= 0 && azimuth_radians <= kTwoPiDouble);
  DCHECK(altitude_radians >= 0 && altitude_radians <= kPiOverTwoDouble);
  if (altitude_radians != 0) {
    // Not using std::round because we need Javascript Math.round behaviour
    // here which is different
    return std::floor(
        Rad2deg(atan(cos(azimuth_radians) / tan(altitude_radians))) + 0.5);
  }

  if (azimuth_radians == kPiOverTwoDouble ||
      azimuth_radians == 3 * kPiOverTwoDouble) {
    return 0;
  } else if (azimuth_radians < kPiOverTwoDouble ||
             azimuth_radians > 3 * kPiOverTwoDouble) {
    // In 1st or 4th quadrant
    return 90;
  } else {
    // In 2nd or 3rd quadrant
    return -90;
  }
}

// static
int32_t PointerEventUtil::TiltYFromSpherical(double azimuth_radians,
                                             double altitude_radians) {
  DCHECK(azimuth_radians >= 0 && azimuth_radians <= kTwoPiDouble);
  DCHECK(altitude_radians >= 0 && altitude_radians <= kPiOverTwoDouble);
  if (altitude_radians != 0) {
    // Not using std::round because we need Javascript Math.round behaviour
    // here which is different
    return std::floor(
        Rad2deg(atan(sin(azimuth_radians) / tan(altitude_radians))) + 0.5);
  }
  if (azimuth_radians == 0 || azimuth_radians == kPiDouble ||
      azimuth_radians == kTwoPiDouble) {
    return 0;
  } else if (azimuth_radians < kPiDouble) {
    // 1st and 2nd quadrants
    return 90;
  } else {
    // 3rd and 4th quadrants
    return -90;
  }
}

// static
int32_t PointerEventUtil::TransformToTiltInValidRange(int32_t tilt_degrees) {
  if (tilt_degrees >= -90 && tilt_degrees <= 90) {
    return tilt_degrees;
  }
  // In order to avoid floating point division we'll make the assumption
  // that |tilt_degrees| will NOT be far outside the valid range.
  // With this assumption we can use loops and integer calculation to transform
  // |tilt_degrees| into valid range.
  while (tilt_degrees > 90) {
    tilt_degrees -= 180;
  }
  while (tilt_degrees < -90) {
    tilt_degrees += 180;
  }

  DCHECK(tilt_degrees >= -90 && tilt_degrees <= 90);
  return tilt_degrees;
}

// static
double PointerEventUtil::TransformToAzimuthInValidRange(
    double azimuth_radians) {
  if (azimuth_radians >= 0 && azimuth_radians <= kTwoPiDouble) {
    return azimuth_radians;
  }
  // In order to avoid floating point division/multiplication we'll make the
  // assumption that |azimuth_radians| will NOT be far outside the valid range.
  // With this assumption we can use loops and addition/subtraction to
  // transform |azimuth_radians| into valid range.
  while (azimuth_radians > kTwoPiDouble) {
    azimuth_radians -= kTwoPiDouble;
  }
  while (azimuth_radians < 0) {
    azimuth_radians += kTwoPiDouble;
  }

  DCHECK(azimuth_radians >= 0 && azimuth_radians <= kTwoPiDouble);
  return azimuth_radians;
}

// static
double PointerEventUtil::TransformToAltitudeInValidRange(
    double altitude_radians) {
  if (altitude_radians >= 0 && altitude_radians <= kPiOverTwoDouble) {
    return altitude_radians;
  }
  // In order to avoid floating point division/multiplication we'll make the
  // assumption that |altitude_radians| will NOT be far outside the valid range.
  // With this assumption we can use loops and addition/subtraction to
  // transform |altitude_radians| into valid range
  while (altitude_radians > kPiOverTwoDouble) {
    altitude_radians -= kPiOverTwoDouble;
  }
  while (altitude_radians < 0) {
    altitude_radians += kPiOverTwoDouble;
  }

  DCHECK(altitude_radians >= 0 && altitude_radians <= kPiOverTwoDouble);
  return altitude_radians;
}
}  // namespace blink
