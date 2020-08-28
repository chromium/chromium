// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore.h"

#include "device/vr/android/arcore/arcore_math_utils.h"

namespace device {

gfx::Transform ArCore::GetCameraUvFromScreenUvTransform() const {
  //
  // Observe how kInputCoordinatesForTransform are transformed by ArCore,
  // compute a matrix based on that and post-multiply with a matrix that
  // performs a Y-flip.
  //
  // We need to add a Y flip because ArCore's
  // AR_COORDINATES_2D_TEXTURE_NORMALIZED coordinates have the origin at the top
  // left to match 2D Android APIs, so it needs a Y flip to get an origin at
  // bottom left as used for textures.
  // The post-multiplied matrix is performing a mapping: (x, y) -> (x, 1 - y).
  //
  return MatrixFromTransformedPoints(
             TransformDisplayUvCoords(kInputCoordinatesForTransform)) *
         gfx::Transform(1, 0, 0, -1, 0, 1);
}

}  // namespace device
