// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/isolated_xr_device/xr_device_service.h"

#include "base/check_is_test.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/services/isolated_xr_device/xr_runtime_provider.h"
#include "device/vr/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/com_init_check_hook.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_OPENXR)
#include "device/vr/openxr/openxr_platform_helper.h"
#endif  // BUILDFLAG(ENABLE_OPENXR)

namespace device {

XrDeviceService::XrDeviceService(
    mojo::PendingReceiver<device::mojom::XRDeviceService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {
#if BUILDFLAG(IS_WIN)
  base::win::ComInitCheckHook::DisableCOMChecksForProcess();
#endif  // BUILDFLAG(IS_WIN)
}

XrDeviceService::~XrDeviceService() = default;

void XrDeviceService::BindRuntimeProvider(
    mojo::PendingReceiver<mojom::IsolatedXRRuntimeProvider> receiver,
    mojo::PendingRemote<mojom::XRDeviceServiceHost> device_service_host) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<IsolatedXRRuntimeProvider>(
          std::move(device_service_host), io_task_runner_),
      std::move(receiver));
}

void XrDeviceService::BindHookForTesting(
    mojo::ScopedMessagePipeHandle receiver) {
  CHECK_IS_TEST();
#if BUILDFLAG(ENABLE_OPENXR)
  OpenXrPlatformHelper::BindHookForTesting(std::move(receiver));
#endif  // BUILDFLAG(ENABLE_OPENXR)
}

}  // namespace device
