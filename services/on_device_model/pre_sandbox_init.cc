// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "services/on_device_model/on_device_model_service.h"

#if defined(ENABLE_ML_INTERNAL)
#include "services/on_device_model/ml/chrome_ml.h"  // nogncheck
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "gpu/config/gpu_info_collector.h"                    // nogncheck
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

}  // namespace

// static
bool OnDeviceModelService::PreSandboxInit() {
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Warm any relevant drivers before attempting to bring up the sandbox. For
  // good measure we initialize a device instance for any adapter with an
  // appropriate backend on top of any integrated or discrete GPU.
  dawnProcSetProcs(&dawn::native::GetProcs());
  auto instance = std::make_unique<dawn::native::Instance>();
  const wgpu::RequestAdapterOptions adapter_options{
      .backendType = wgpu::BackendType::Vulkan,
  };
  std::vector<dawn::native::Adapter> adapters =
      instance->EnumerateAdapters(&adapter_options);
  for (auto& adapter : adapters) {
    wgpu::AdapterInfo info;
    adapter.GetInfo(&info);
    if (info.adapterType == wgpu::AdapterType::IntegratedGPU ||
        info.adapterType == wgpu::AdapterType::DiscreteGPU) {
      const wgpu::DeviceDescriptor descriptor;
      wgpu::Device device{adapter.CreateDevice(&descriptor)};
      if (device) {
        device.Destroy();
      }
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return true;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
void OnDeviceModelService::AddSandboxLinuxOptions(
    sandbox::policy::SandboxLinux::Options& options) {
  // Make sure any necessary vendor-specific options are set.
  gpu::GPUInfo info;
  gpu::CollectBasicGraphicsInfo(&info);
  UpdateSandboxOptionsForGpu(info.gpu, options);
  for (const auto& gpu : info.secondary_gpus) {
    UpdateSandboxOptionsForGpu(gpu, options);
  }
}
#endif

// static
bool OnDeviceModelService::Shutdown() {
  return true;
}

}  // namespace on_device_model
