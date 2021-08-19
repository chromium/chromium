// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_utils.h"

#include "ui/gfx/transform.h"

namespace device {
namespace gvr_utils {

void GvrMatToTransform(const gvr::Mat4f& in, gfx::Transform* out) {
  *out = gfx::Transform(in.m[0][0], in.m[0][1], in.m[0][2], in.m[0][3],
                        in.m[1][0], in.m[1][1], in.m[1][2], in.m[1][3],
                        in.m[2][0], in.m[2][1], in.m[2][2], in.m[2][3],
                        in.m[3][0], in.m[3][1], in.m[3][2], in.m[3][3]);
}

}  // namespace gvr_utils
}  // namespace device
