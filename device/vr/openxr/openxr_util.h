// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_UTIL_H_
#define DEVICE_VR_OPENXR_OPENXR_UTIL_H_

#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// These macros aren't common in Chromium and generally discouraged, so define
// all OpenXR helper macros here so they can be kept track of. This file
// should not be included outside of device/vr/openxr.

// The caller must have a variable of type XrResult named xr_result defined in
// their scope. This macro sets xr_result to |xrcode|.
#define RETURN_IF_XR_FAILED(xrcode) \
  do {                              \
    xr_result = (xrcode);           \
    if (XR_FAILED(xr_result))       \
      return xr_result;             \
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

XrResult GetSystem(XrInstance instance, XrSystemId* system);

XrResult CreateInstance(XrInstance* instance);

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_UTIL_H_
