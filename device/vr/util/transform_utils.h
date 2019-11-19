// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_UTIL_TRANSFORM_UTILS_H_
#define DEVICE_VR_UTIL_TRANSFORM_UTILS_H_

namespace gfx {
class Transform;
class Vector3dF;
}  // namespace gfx

namespace device {
namespace vr_utils {

gfx::Transform MakeTranslationTransform(float x, float y, float z);
gfx::Transform MakeTranslationTransform(const gfx::Vector3dF& translation);
gfx::Transform DefaultHeadFromLeftEyeTransform();
gfx::Transform DefaultHeadFromRightEyeTransform();

}  // namespace vr_utils
}  // namespace device

#endif  // DEVICE_VR_UTIL_TRANSFORM_UTILS_H_
