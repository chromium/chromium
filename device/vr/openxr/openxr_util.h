// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_UTIL_H_
#define DEVICE_VR_OPENXR_OPENXR_UTIL_H_

#include <d3d11.h>
#include <vector>

#include "base/logging.h"
#include "base/types/id_type.h"
#include "device/vr/openxr/openxr_defs.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

using AnchorId = base::IdTypeU64<class AnchorTag>;
constexpr AnchorId kInvalidAnchorId;

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

#define RETURN_IF_FALSE(condition, error_code, msg) \
  do {                                              \
    if (!(condition)) {                             \
      LOG(ERROR) << __FUNCTION__ << ": " << msg;    \
      return error_code;                            \
    }                                               \
  } while (false)

#define RETURN_IF(condition, error_code, msg)    \
  do {                                           \
    if (condition) {                             \
      LOG(ERROR) << __FUNCTION__ << ": " << msg; \
      return error_code;                         \
    }                                            \
  } while (false)

// Returns the identity pose, where the position is {0, 0, 0} and the
// orientation is {0, 0, 0, 1}.
XrPosef PoseIdentity();
gfx::Transform XrPoseToGfxTransform(const XrPosef& pose);
XrPosef GfxTransformToXrPose(const gfx::Transform& transform);
bool IsPoseValid(XrSpaceLocationFlags locationFlags);

XrResult GetSystem(XrInstance instance, XrSystemId* system);

XrResult CreateInstance(
    XrInstance* instance,
    const OpenXrExtensionEnumeration& extension_enumeration);

std::vector<XrEnvironmentBlendMode> GetSupportedBlendModes(XrInstance instance,
                                                           XrSystemId system);

// Insert an extension struct into the next chain of an xrStruct
template <typename XrStruct, typename XrExtension>
void InsertExtensionStruct(XrStruct& xrStruct, XrExtension& xrExtension) {
  xrExtension.next = xrStruct.next;
  xrStruct.next = &xrExtension;
}

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_UTIL_H_
