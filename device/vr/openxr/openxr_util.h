// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_UTIL_H_
#define DEVICE_VR_OPENXR_OPENXR_UTIL_H_

#include "base/logging.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "device/vr/public/mojom/xr_session.mojom-forward.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace device {
// These macros aren't common in Chromium and generally discouraged, so define
// all OpenXR helper macros here so they can be kept track of. This file
// should not be included outside of device/vr/openxr.

#define RETURN_IF_XR_FAILED(xrcode)                                     \
  do {                                                                  \
    XrResult return_if_xr_failed_xr_result = (xrcode);                  \
    if (XR_FAILED(return_if_xr_failed_xr_result)) {                     \
      DLOG(ERROR) << __func__                                           \
                  << " Failed with: " << return_if_xr_failed_xr_result; \
      return return_if_xr_failed_xr_result;                             \
    }                                                                   \
  } while (false)

#define RETURN_VAL_IF_XR_FAILED(xrcode, return_expr)                    \
  do {                                                                  \
    XrResult return_if_xr_failed_xr_result = (xrcode);                  \
    if (XR_FAILED(return_if_xr_failed_xr_result)) {                     \
      DLOG(ERROR) << __func__                                           \
                  << " Failed with: " << return_if_xr_failed_xr_result; \
      return return_expr;                                               \
    }                                                                   \
  } while (false)

#define RETURN_IF_FALSE(condition, error_code, msg) \
  do {                                              \
    if (!(condition)) {                             \
      DLOG(ERROR) << __func__ << ": " << msg;       \
      return error_code;                            \
    }                                               \
  } while (false)

#define RETURN_IF(condition, error_code, msg) \
  do {                                        \
    if (condition) {                          \
      DLOG(ERROR) << __func__ << ": " << msg; \
      return error_code;                      \
    }                                         \
  } while (false)

// Returns the identity pose, where the position is {0, 0, 0} and the
// orientation is {0, 0, 0, 1}.
XrPosef PoseIdentity();
gfx::Transform XrPoseToGfxTransform(const XrPosef& pose);
device::Pose XrPoseToDevicePose(const XrPosef& pose);
XrPosef GfxTransformToXrPose(const gfx::Transform& transform);
mojom::VRFieldOfViewPtr XrFovToMojomFov(const XrFovf& xr_fov);
bool IsPoseValid(XrSpaceLocationFlags locationFlags);

bool IsFeatureSupportedForMode(device::mojom::XRSessionFeature feature,
                               device::mojom::XRSessionMode mode);

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_UTIL_H_
