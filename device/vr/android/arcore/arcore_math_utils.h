// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_MATH_UTILS_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_MATH_UTILS_H_

#include <array>
#include <vector>

#include "base/containers/span.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

// Creates a matrix that transforms UV coordinates based on how a well known
// input was transformed. ArCore doesn't provide a way to get a matrix directly.
// There's a function to transform UV vectors individually, which can't be used
// from a shader, so we run that on selected well-known vectors
// (kDisplayCoordinatesForTransform) and recreate the matrix from the result.
gfx::Transform MatrixFromTransformedPoints(const base::span<const float> uvs);

// Input coordinates used when computing UV transform.
// |MatrixFromTransformedPoints(uvs)| function above assumes that the |uvs| are
// the result of transforming kInputCoordinatesForTransform by some matrix.
constexpr std::array<float, 6> kInputCoordinatesForTransform = {0.f, 0.f, 1.f,
                                                                0.f, 0.f, 1.f};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_MATH_UTILS_H_
