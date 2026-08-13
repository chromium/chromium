// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/isolated_xr_device/xr_service_test_hook.h"

#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENXR)
#include "device/vr/openxr/openxr_api_wrapper.h"
#endif  // BUILDFLAG(ENABLE_OPENXR)

namespace device {

void XRServiceTestHook::SetTestHook(
    mojo::PendingRemote<device_test::mojom::XRTestHook> hook,
    device_test::mojom::XRServiceTestHook::SetTestHookCallback callback) {
#if BUILDFLAG(ENABLE_OPENXR)
  OpenXrApiWrapper::SetTestHook(std::move(hook));
#endif  // BUILDFLAG(ENABLE_OPENXR)

  std::move(callback).Run();
}

XRServiceTestHook::~XRServiceTestHook() = default;

XRServiceTestHook::XRServiceTestHook() = default;

}  // namespace device
