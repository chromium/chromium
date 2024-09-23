// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/browser/xr/webxr_internals/webxr_internals_handler_impl.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"

namespace content {

namespace {

gpu::GPUInfo GetGPUInfo() {
  content::GpuDataManager* gpu_data_manager =
      content::GpuDataManager::GetInstance();
  const gpu::GPUInfo gpu_info = gpu_data_manager->GetGPUInfo();
  return gpu_info;
}

std::string GetOSVersion() {
  std::string os_version;

#if BUILDFLAG(IS_WIN)
  std::string win_version = base::SysInfo::OperatingSystemVersion();
  base::ReplaceSubstringsAfterOffset(&win_version, 0, " SP", ".");
  os_version = win_version;
#else
  // Every other OS is supported by OperatingSystemVersionNumbers
  int major, minor, build;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &build);
  os_version = base::StringPrintf("%d.%d.%d", major, minor, build);
#endif

  return os_version;
}
}  // namespace

WebXrInternalsHandlerImpl::WebXrInternalsHandlerImpl(
    mojo::PendingReceiver<webxr::mojom::WebXrInternalsHandler> receiver,
    WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      runtime_manager_(
          XRRuntimeManagerImpl::GetOrCreateInstance(*web_contents)) {}

WebXrInternalsHandlerImpl::~WebXrInternalsHandlerImpl() = default;

void WebXrInternalsHandlerImpl::GetDeviceInfo(GetDeviceInfoCallback callback) {
  webxr::mojom::DeviceInfoPtr info = webxr::mojom::DeviceInfo::New();

  // OS
  info->operating_system_name = base::SysInfo::OperatingSystemName();
  info->operating_system_version = GetOSVersion();

  // GPU
  const gpu::GPUInfo gpu_info = GetGPUInfo();
  info->gpu_gl_vendor = gpu_info.gl_vendor;
  info->gpu_gl_renderer = gpu_info.gl_renderer;

  std::move(callback).Run(std::move(info));
}

void WebXrInternalsHandlerImpl::GetActiveRuntimes(
    GetActiveRuntimesCallback callback) {
  std::vector<webxr::mojom::RuntimeInfoPtr> info =
      runtime_manager_->GetActiveRuntimes();
  std::move(callback).Run(std::move(info));
}

void WebXrInternalsHandlerImpl::SubscribeToEvents(
    mojo::PendingRemote<webxr::mojom::XRInternalsSessionListener>
        pending_remote) {
  runtime_manager_->GetLoggerManager().SubscribeToEvents(
      std::move(pending_remote));
}

}  // namespace content
