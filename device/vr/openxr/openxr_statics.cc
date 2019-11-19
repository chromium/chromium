// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_statics.h"

#include "device/vr/openxr/openxr_util.h"

namespace device {

OpenXrStatics::OpenXrStatics() : instance_(XR_NULL_HANDLE) {}

OpenXrStatics::~OpenXrStatics() {
  if (instance_ != XR_NULL_HANDLE) {
    xrDestroyInstance(instance_);
    instance_ = XR_NULL_HANDLE;
  }
}

bool OpenXrStatics::IsHardwareAvailable() {
  if (instance_ == XR_NULL_HANDLE && XR_FAILED(CreateInstance(&instance_))) {
    return false;
  }

  XrSystemId system;
  return XR_SUCCEEDED(GetSystem(instance_, &system));
}

bool OpenXrStatics::IsApiAvailable() {
  return instance_ != XR_NULL_HANDLE ||
         XR_SUCCEEDED(CreateInstance(&instance_));
}

}  // namespace device