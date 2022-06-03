// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_transformation.h"

#include <math.h>
#include <stddef.h>

#include "base/notreached.h"

namespace media {
namespace {

double FixedToFloatingPoint(int32_t i) {
  return static_cast<double>(i >> 16);
}

}  // namespace

std::string VideoRotationToString(VideoRotation rotation) {
  switch (rotation) {
    case VIDEO_ROTATION_0:
      return "0°";
    case VIDEO_ROTATION_90:
      return "90°";
    case VIDEO_ROTATION_180:
      return "180°";
    case VIDEO_ROTATION_270:
      return "270°";
  }
  NOTREACHED();
  return "";
}

bool operator==(const struct VideoTransformation& first,
                const struct VideoTransformation& second) {
  return first.rotation == second.rotation && first.mirrored == second.mirrored;
}

VideoTransformation::VideoTransformation(int32_t matrix[4]) {
  // Rotation by angle Θ is represented in the matrix as:
  // [ cos(Θ), -sin(Θ)]
  // [ sin(Θ),  cos(Θ)]
  // A vertical flip is represented by the cosine's having opposite signs
  // and a horizontal flip is represented by the sine's having the same sign.

  // Check the matrix for validity
  if (abs(matrix[0]) != abs(matrix[3]) || abs(matrix[1]) != abs(matrix[2])) {
    rotation = VIDEO_ROTATION_0;
    mirrored = false;
    return;
  }

  double angle = acos(FixedToFloatingPoint(matrix[0])) * 180 / base::kPiDouble;

  // Calculate angle offsets for rotation - rotating about the X axis
  // can be expressed as a 180 degree rotation and a Y axis rotation
  mirrored = false;
  if (matrix[0] != matrix[3] && matrix[0] != 0) {
    mirrored = !mirrored;
    angle += 180;
  }

  if (matrix[1] == matrix[3] && matrix[1] != 0) {
    mirrored = !mirrored;
  }

  // Normalize the angle
  while (angle < 0)
    angle += 360;

  while (angle >= 360)
    angle -= 360;

  // 16 bits of fixed point decimal is enough to give 6 decimals of precision
  // to cos(Θ). A delta of ±0.000001 causes acos(cos(Θ)) to differ by a minimum
  // of 0.0002, which is why we only need to check that the angle is only
  // accurate to within four decimal places. This is preferred to checking for
  // a more precise accuracy, as the 'double' type is architecture dependent and
  // there may be variance in floating point errors.
  if (abs(angle - 0) < 1e-4) {
    rotation = VIDEO_ROTATION_0;
  } else if (abs(angle - 180) < 1e-4) {
    rotation = VIDEO_ROTATION_180;
  } else if (abs(angle - 90) < 1e-4) {
    bool quadrant = asin(FixedToFloatingPoint(matrix[2])) < 0;
    rotation = quadrant ? VIDEO_ROTATION_90 : VIDEO_ROTATION_270;
  } else {
    rotation = VIDEO_ROTATION_0;
    mirrored = false;
  }
}

}  // namespace media
