// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_init_ozone.h"

#include "base/check.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/command_buffer/service/drm_modifiers_filter_vulkan.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN) && BUILDFLAG(IS_CHROMEOS)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/drm_modifiers_filter_dawn.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"  // nogncheck
#endif



namespace gpu::gpu_init_internal {

void InitializePlatformForGpu(bool enable_native_gpu_memory_buffers,
                              bool single_process) {
  // Initialize Ozone GPU after the watchdog in case it hangs. The sandbox
  // may also have started at this point.
  ui::OzonePlatform::InitParams params;
  params.single_process = single_process;
  params.enable_native_gpu_memory_buffers = enable_native_gpu_memory_buffers;

#if BUILDFLAG(IS_CHROMEOS)
  params.allow_sync_and_real_buffer_page_flip_testing = true;
#endif  // BUILDFLAG(IS_CHROMEOS)
  ui::OzonePlatform::InitializeForGPU(params);
}

void MaybeDisableWebGPUOnVulkanViaGLInterop(GpuFeatureInfo& gpu_feature_info) {
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .webgpu_on_vulkan_via_gl_interop) {
    gpu_feature_info
        .status_values[GPU_FEATURE_TYPE_WEBGPU_ON_VK_VIA_GL_INTEROP] =
        kGpuFeatureStatusDisabled;
  }
}

void QueryNativePixmapSupport(GpuFeatureInfo& gpu_feature_info) {
  // We need to get supported formats before sandboxing to avoid an known
  // issue which breaks the camera preview. (b/166850715)
  TRACE_EVENT("gpu,startup", "ui::ozone::CanImportNativePixmap");
  auto* surface_factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  auto* gl_ozone = surface_factory->GetCurrentGLOzone();
  if (!gl_ozone) {
    return;
  }

  gpu_feature_info.supports_nv12_gl_native_pixmap =
      gl_ozone->CanImportNativePixmap(viz::MultiPlaneFormat::kNV12);
  gpu_feature_info.supports_p010_gl_native_pixmap =
      gl_ozone->CanImportNativePixmap(viz::MultiPlaneFormat::kP010);
}

void AfterSandboxEntryAndSetDrmModifiersFilter(
    GpuFeatureInfo& gpu_feature_info,
    const SetDrmModifiersFilterParams& params) {
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();
  [[maybe_unused]] auto* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  bool filter_set = false;
#if BUILDFLAG(ENABLE_VULKAN)
  if (gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN] ==
          kGpuFeatureStatusEnabled &&
      factory->SupportsDrmModifiersFilter()) {
    CHECK(!filter_set);
    DCHECK(params.vulkan_implementation &&
           params.vulkan_implementation->GetVulkanInstance() &&
           params.vulkan_implementation->GetVulkanInstance()->vk_instance() !=
               VK_NULL_HANDLE);
    factory->SetDrmModifiersFilter(std::make_unique<DrmModifiersFilterVulkan>(
        params.vulkan_implementation));
    filter_set = true;
  }
#endif  // BUILDFLAG(ENABLE_VULKAN)
#if BUILDFLAG(SKIA_USE_DAWN) && BUILDFLAG(IS_CHROMEOS)
  if (params.dawn_context_provider && factory->SupportsDrmModifiersFilter()) {
    CHECK(!filter_set);
    factory->SetDrmModifiersFilter(std::make_unique<DrmModifiersFilterDawn>(
        params.dawn_context_provider->GetDevice().GetAdapter()));
    filter_set = true;
  }
#endif  // BUILDFLAG(SKIA_USE_DAWN) && BUILDFLAG(IS_CHROMEOS)
}

} // namespace gpu::gpu_init_internal


