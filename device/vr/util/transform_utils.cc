// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/transform_utils.h"

#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {
namespace vr_utils {

gfx::Transform MakeTranslationTransform(float x, float y, float z) {
  gfx::DecomposedTransform decomp;
  decomp.translate[0] = x;
  decomp.translate[1] = y;
  decomp.translate[2] = z;
  return gfx::ComposeTransform(decomp);
}

gfx::Transform MakeTranslationTransform(const gfx::Vector3dF& translation) {
  return MakeTranslationTransform(translation.x(), translation.y(),
                                  translation.z());
}

constexpr float kDefaultIPD = 0.1f;  // 10cm

gfx::Transform DefaultHeadFromLeftEyeTransform() {
  return MakeTranslationTransform(-kDefaultIPD * 0.5, 0, 0);
}

gfx::Transform DefaultHeadFromRightEyeTransform() {
  return MakeTranslationTransform(kDefaultIPD * 0.5, 0, 0);
}

}  // namespace vr_utils
}  // namespace device
