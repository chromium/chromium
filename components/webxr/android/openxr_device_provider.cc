// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/openxr_device_provider.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/webxr/android/openxr_platform_helper_android.h"
#include "components/webxr/android/xr_session_coordinator.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "device/vr/android/xr_image_transport_base.h"
#include "device/vr/openxr/context_provider_callbacks.h"
#include "device/vr/openxr/openxr_device.h"
#include "device/vr/openxr/openxr_platform.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace webxr {

OpenXrDeviceProvider::OpenXrDeviceProvider() = default;

OpenXrDeviceProvider::~OpenXrDeviceProvider() {
  // Must make sure that the OpenXrPlatformHelper outlives the OpenXrDevice.
  openxr_device_.reset();
  openxr_platform_helper_.reset();
}

void OpenXrDeviceProvider::Initialize(
    device::VRDeviceProviderClient* client,
    content::WebContents* initializing_web_contents) {
  CHECK(!initialized_);

  // TODO(crbug.com/40917172): Support non-shared buffer rendering path.
  if (device::XrImageTransportBase::UseSharedBuffer()) {
    openxr_platform_helper_ = std::make_unique<OpenXrPlatformHelperAndroid>();

    if (openxr_platform_helper_->EnsureInitialized() &&
        openxr_platform_helper_->CheckHardwareSupport(
            initializing_web_contents)) {
      DVLOG(2) << __func__ << ": OpenXr is supported, creating device";
      // Unretained is safe since we own the device this callback is being
      // passed to and we ensure that it does not outlive us. The device is
      // expected to wind down any threads that it spins up as well (for e.g.
      // rendering), so this destruction is also safe. The OpenXrDevice passes
      // this off to different render loops as it creates them, so we can't just
      // use a WeakPtr of ourselves here, since it would technically end up
      // dereferenced on different threads (albeit all children).
      openxr_device_ = std::make_unique<device::OpenXrDevice>(
          base::BindRepeating(&OpenXrDeviceProvider::CreateContextProviderAsync,
                              base::Unretained(this)),
          openxr_platform_helper_.get());

      client->AddRuntime(openxr_device_->GetId(),
                         openxr_device_->GetDeviceData(),
                         openxr_device_->BindXRRuntime());
    } else {
      DVLOG(2) << __func__ << ": No OpenXR Hardware found.";
    }
  }

  initialized_ = true;
  client->OnProviderInitialized();
}

bool OpenXrDeviceProvider::Initialized() {
  return initialized_;
}

void OpenXrDeviceProvider::CreateContextProviderAsync(
    VizContextProviderCallback viz_context_provider_callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](int surface_handle,
             content::Compositor::ContextProviderCallback callback) {
            content::Compositor::CreateContextProvider(
                surface_handle, gpu::SharedMemoryLimits::ForMailboxContext(),
                std::move(callback));
          },
          gpu::kNullSurfaceHandle, std::move(viz_context_provider_callback)));
}

}  // namespace webxr
