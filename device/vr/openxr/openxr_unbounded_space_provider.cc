// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_unbounded_space_provider.h"

#include "base/check.h"
#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
OpenXrUnboundedSpaceProvider::OpenXrUnboundedSpaceProvider() = default;
OpenXrUnboundedSpaceProvider::~OpenXrUnboundedSpaceProvider() = default;

XrResult OpenXrUnboundedSpaceProvider::CreateSpace(XrSession session,
                                                   XrSpace* space) {
  CHECK(session != XR_NULL_HANDLE);
  XrReferenceSpaceCreateInfo space_create_info = {
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  space_create_info.referenceSpaceType = GetType();
  space_create_info.poseInReferenceSpace = PoseIdentity();

  return xrCreateReferenceSpace(session, &space_create_info, space);
}
}  // namespace device
