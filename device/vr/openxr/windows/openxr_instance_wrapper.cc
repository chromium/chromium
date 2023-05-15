// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/windows/openxr_instance_wrapper.h"

#include "base/check.h"
#include "base/memory/singleton.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXrInstanceWrapper* OpenXrInstanceWrapper::GetWrapper() {
  return base::Singleton<
      OpenXrInstanceWrapper,
      base::LeakySingletonTraits<OpenXrInstanceWrapper>>::get();
}

bool OpenXrInstanceWrapper::HasXrInstance() {
  return instance_ != XR_NULL_HANDLE;
}

XrInstance OpenXrInstanceWrapper::GetXrInstance() {
  return instance_;
}

void OpenXrInstanceWrapper::SetXrInstance(XrInstance instance) {
  CHECK(!HasXrInstance() || instance == XR_NULL_HANDLE);
  instance_ = instance;
}

}  // namespace device
