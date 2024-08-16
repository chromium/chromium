// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/video_transformation.h"

#include <math.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "base/strings/string_number_conversions.h"

namespace media {
namespace {

template <size_t decimal_bits>
double FixedToFloatingPoint(int64_t i) {
  return i / static_cast<double>(1 << decimal_bits);
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
}

bool operator==(const struct VideoTransformation& first,
                const struct VideoTransformation& second) {
  return first.rotation == second.rotation && first.mirrored == second.mirrored;
}

// static
VideoTransformation VideoTransformation::FromFFmpegDisplayMatrix(
    const int32_t* matrix3x3) {
  const int32_t matrix2x2[4] = {
      matrix3x3[0],
      matrix3x3[1],
      matrix3x3[3],
      matrix3x3[4],
  };
  return VideoTransformation(matrix2x2);
}

VideoTransformation::VideoTransformation(const int32_t matrix[4]) {
  // Promote to int64_t to avoid abs(int32_min) being undefined.
  const int64_t matrix64[4] = {matrix[0], matrix[1], matrix[2], matrix[3]};

  // Rotation by angle Θ is represented in the matrix as:
  // [ cos(Θ), -sin(Θ)]
  // [ sin(Θ),  cos(Θ)]
  // A vertical flip is represented by the cosine's having opposite signs
  // and a horizontal flip is represented by the sine's having the same sign.
  // Check the matrix for validity
  if (abs(matrix64[0]) != abs(matrix64[3]) ||
      abs(matrix64[1]) != abs(matrix64[2])) {
    rotation = VIDEO_ROTATION_0;
    mirrored = false;
    return;
  }

  double angle = base::RadToDeg(acos(FixedToFloatingPoint<16>(matrix64[0])));
  double check_angle =
      base::RadToDeg(asin(FixedToFloatingPoint<16>(matrix64[1])));
  double offset = abs(abs(angle) - abs(check_angle));
  while (offset >= 180.0)
    offset -= 180.0;

  if (offset > 1e-3) {
    rotation = VIDEO_ROTATION_0;
    mirrored = false;
    return;
  }

  // Calculate angle offsets for rotation - rotating about the X axis
  // can be expressed as a 180 degree rotation and a Y axis rotation
  mirrored = false;
  if (matrix64[0] != matrix64[3] && matrix64[0] != 0) {
    mirrored = !mirrored;
    angle += 180;
  }

  if (matrix64[1] == matrix64[3] && matrix64[1] != 0) {
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
    rotation = (check_angle > 0) ? VIDEO_ROTATION_90 : VIDEO_ROTATION_270;
  } else {
    rotation = VIDEO_ROTATION_0;
    mirrored = false;
  }
}

}  // namespace media
