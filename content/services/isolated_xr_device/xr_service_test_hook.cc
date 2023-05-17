// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/isolated_xr_device/xr_service_test_hook.h"

#include "base/functional/bind.h"
#include "base/process/process.h"
#include "content/services/isolated_xr_device/xr_test_hook_wrapper.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENXR)
#include "device/vr/openxr/openxr_api_wrapper.h"
#endif  // BUIDLFLAG(ENABLE_OPENXR)

namespace {

void UnsetTestHook(std::unique_ptr<device::XRTestHookWrapper> wrapper) {
  // Unset the testhook wrapper with the VR runtimes,
  // so any future calls to them don't use it.
#if BUILDFLAG(ENABLE_OPENXR)
  device::OpenXrApiWrapper::SetTestHook(nullptr);
#endif  // BUILDFLAG(ENABLE_OPENXR)
}

}  // namespace

namespace device {

void XRServiceTestHook::SetTestHook(
    mojo::PendingRemote<device_test::mojom::XRTestHook> hook,
    device_test::mojom::XRServiceTestHook::SetTestHookCallback callback) {
  // Create a new wrapper (or use null)
  std::unique_ptr<XRTestHookWrapper> wrapper =
      hook ? std::make_unique<XRTestHookWrapper>(std::move(hook)) : nullptr;

#if BUILDFLAG(ENABLE_OPENXR)
  OpenXrApiWrapper::SetTestHook(wrapper.get());
#endif  // BUILDFLAG(ENABLE_OPENXR)

  // Store the new wrapper, so we keep it alive.
  wrapper_ = std::move(wrapper);

  std::move(callback).Run();
}

void XRServiceTestHook::TerminateDeviceServiceProcessForTesting(
    DeviceCrashCallback callback) {
  base::Process::TerminateCurrentProcessImmediately(1);
}

XRServiceTestHook::~XRServiceTestHook() {
  // If we have an existing wrapper, and it is bound to a thread, post a message
  // to destroy it on that thread.
  if (wrapper_) {
    auto runner = wrapper_->GetBoundTaskRunner();
    if (runner) {
      runner->PostTask(FROM_HERE,
                       base::BindOnce(UnsetTestHook, std::move(wrapper_)));
    } else {
      UnsetTestHook(std::move(wrapper_));
    }
  }
}

XRServiceTestHook::XRServiceTestHook() = default;

}  // namespace device
