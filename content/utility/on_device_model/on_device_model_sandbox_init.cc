// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/on_device_model/on_device_model_sandbox_init.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"

#if defined(ENABLE_ML_INTERNAL)
#include "services/on_device_model/ml/chrome_ml.h"      // nogncheck
#include "services/on_device_model/ml/gpu_blocklist.h"  // nogncheck
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <errno.h>

#include "content/common/gpu_pre_sandbox_hook_linux.h"
#include "gpu/config/gpu_info_collector.h"  // nogncheck
#include "sandbox/policy/linux/sandbox_linux.h"
#endif

#if !BUILDFLAG(IS_FUCHSIA) && \
    !(BUILDFLAG(IS_LINUX) && BUILDFLAG(ENABLE_CAST_RECEIVER))
#include "base/feature_list.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"          // nogncheck
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#include "third_party/dawn/include/dawn/webgpu_cpp.h"         // nogncheck
#endif

namespace on_device_model {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
constexpr uint32_t kVendorIdAMD = 0x1002;
constexpr uint32_t kVendorIdIntel = 0x8086;
constexpr uint32_t kVendorIdNVIDIA = 0x10DE;
constexpr uint32_t kVendorIdVirtIO = 0x1AF4;

void UpdateSandboxOptionsForGpu(
    const gpu::GPUInfo::GPUDevice& device,
    sandbox::policy::SandboxLinux::Options& options) {
  switch (device.vendor_id) {
    case kVendorIdAMD:
      options.use_amd_specific_policies = true;
      break;
    case kVendorIdIntel:
      options.use_intel_specific_policies = true;
      break;
    case kVendorIdNVIDIA:
      options.use_nvidia_specific_policies = true;
      break;
    case kVendorIdVirtIO:
      options.use_virtio_specific_policies = true;
      break;
    default:
      break;
  }
}
#endif

#if !BUILDFLAG(IS_FUCHSIA) && \
    !(BUILDFLAG(IS_LINUX) && BUILDFLAG(ENABLE_CAST_RECEIVER))
// If this feature is enabled, a WebGPU device is created for each valid
// adapter. This makes sure any relevant drivers or other libs are loaded before
// enabling the sandbox.
BASE_FEATURE(kOnDeviceModelWarmDrivers,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
#endif

bool ShouldWarmDrivers() {
#if BUILDFLAG(IS_FUCHSIA) || \
    (BUILDFLAG(IS_LINUX) && BUILDFLAG(ENABLE_CAST_RECEIVER))
  return false;
#else
  bool is_gpu_not_blocklisted = true;
#if defined(ENABLE_ML_INTERNAL)
  ml::DeviceInfo device_info =
      ml::QueryDeviceInfo(ml::ChromeML::Get()->api(), /*log_histogram=*/false);
  is_gpu_not_blocklisted =
      device_info.gpu_blocked_reason == ml::GpuBlockedReason::kNotBlocked;
#endif
  return base::FeatureList::IsEnabled(kOnDeviceModelWarmDrivers) &&
         is_gpu_not_blocklisted;
#endif
}

}  // namespace

bool PreSandboxInit() {
#if defined(ENABLE_ML_INTERNAL)
  // Ensure the library is loaded before the sandbox is initialized.
  if (!ml::ChromeML::Get()) {
    LOG(ERROR) << "Unable to load ChromeML.";
    return false;
  }
#endif

#if defined(DAWN_USE_BUILT_DXC) && BUILDFLAG(IS_WIN)
  base::FilePath module_path;
  if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
    // Preload DXC requirements if enabled.
    base::LoadNativeLibrary(module_path.Append(L"dxil.dll"), nullptr);
    base::LoadNativeLibrary(module_path.Append(L"dxcompiler.dll"), nullptr);
  }
#endif

  if (ShouldWarmDrivers()) {
    // Warm any relevant drivers before attempting to bring up the sandbox. For
    // good measure we initialize a device instance for any adapter with an
    // appropriate backend on top of any integrated or discrete GPU.
#if !BUILDFLAG(IS_FUCHSIA) && \
    !(BUILDFLAG(IS_LINUX) && BUILDFLAG(ENABLE_CAST_RECEIVER))
    dawnProcSetProcs(&dawn::native::GetProcs());
    auto instance = std::make_unique<dawn::native::Instance>();
    const wgpu::RequestAdapterOptions adapter_options{
#if BUILDFLAG(IS_WIN)
        .backendType = wgpu::BackendType::D3D12,
#elif BUILDFLAG(IS_APPLE)
        .backendType = wgpu::BackendType::Metal,
#else
        .backendType = wgpu::BackendType::Vulkan,
#endif
    };
    std::vector<dawn::native::Adapter> adapters =
        instance->EnumerateAdapters(&adapter_options);
    for (auto& nativeAdapter : adapters) {
      wgpu::Adapter adapter = wgpu::Adapter(nativeAdapter.Get());
      wgpu::AdapterInfo info;
      adapter.GetInfo(&info);
      if (info.adapterType == wgpu::AdapterType::IntegratedGPU ||
          info.adapterType == wgpu::AdapterType::DiscreteGPU) {
        const wgpu::DeviceDescriptor descriptor;
        wgpu::Device device{nativeAdapter.CreateDevice(&descriptor)};
        if (device) {
          device.Destroy();
        }
      }
    }
#endif
  }
  return true;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void AddSandboxLinuxOptions(sandbox::policy::SandboxLinux::Options& options) {
  // Make sure any necessary vendor-specific options are set.
  gpu::GPUInfo info;
  gpu::CollectBasicGraphicsInfo(&info);
  UpdateSandboxOptionsForGpu(info.gpu, options);
  for (const auto& gpu : info.secondary_gpus) {
    UpdateSandboxOptionsForGpu(gpu, options);
  }
}

bool PreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  std::vector<sandbox::syscall_broker::BrokerFilePermission> file_permissions =
      content::FilePermissionsForGpu(options);
  file_permissions.push_back(
      sandbox::syscall_broker::BrokerFilePermission::ReadOnly(
          "/sys/devices/system/cpu/online"));

  sandbox::policy::SandboxLinux::GetInstance()->StartBrokerProcess(
      content::CommandSetForGPU(options), file_permissions, options);

  if (!content::LoadLibrariesForGpu(options)) {
    return false;
  }

  errno = 0;
  return true;
}
#endif

bool Shutdown() {
  return true;
}

}  // namespace on_device_model
