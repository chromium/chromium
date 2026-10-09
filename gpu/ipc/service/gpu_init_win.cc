// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_init_win.h"

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "services/on_device_model/ml_internal_buildflags.h"
#include "skia/buildflags.h"
#include "ui/gl/dc_surface_solid_color_pool.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_features.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/ipc/service/dawn_texture_solid_color_pool.h"
#endif

#if BUILDFLAG(ENABLE_ML_INTERNAL)
#include "services/webnn/public/mojom/features.mojom-features.h"  // nogncheck
#endif

namespace gpu::gpu_init_internal {

bool PreloadDLLs(const base::CommandLine* command_line,
                 const GpuPreferences& gpu_preferences) {
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_MODULE, &module_path)) {
    return false;
  }

  // Preload vk_swiftshader.dll when SwiftShader may be needed:
  // - IsSwiftShaderAllowed() covers ANGLE/GL flags like
  //   --enable-unsafe-swiftshader and --use-angle=swiftshader.
  // - use_webgpu_adapter covers --use-webgpu-adapter=swiftshader for
  //   WebGPU fallback adapter selection.
  // - DefaultForceFallbackAdapter() covers Graphite/Dawn using
  //   SwiftShader via --skia-graphite-dawn-backend=swiftshader.
  if (features::IsSwiftShaderAllowed(command_line) ||
      gpu_preferences.use_webgpu_adapter == WebGPUAdapterName::kSwiftShader ||
      DawnContextProvider::DefaultForceFallbackAdapter()) {
    TRACE_EVENT("gpu,startup", "Load vk_swiftshader.dll");
    base::LoadNativeLibrary(module_path.Append(L"vk_swiftshader.dll"), nullptr);
  }

#if defined(DAWN_USE_BUILT_DXC)
  {
    TRACE_EVENT("gpu,startup", "Load dxcompiler.dll");
    base::LoadNativeLibrary(module_path.Append(L"dxcompiler.dll"), nullptr);
  }
#endif  // defined(DAWN_USE_BUILT_DXC)

#if BUILDFLAG(ENABLE_ML_INTERNAL)
  if (base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebMachineLearningNeuralNetwork)) {
    // Ensure that optimization_guide_internal.dll is loaded before the
    // sandbox is initialized as this provides a GPU delegate used as a
    // fallback when Windows ML is not available.
    TRACE_EVENT("gpu,startup", "Load optimization_guide_internal.dll");
    base::LoadNativeLibrary(
        module_path.Append(L"optimization_guide_internal.dll"), nullptr);
  }
#endif

  return true;
}

void InitializeDirectComposition(DawnContextProvider* dawn_context_provider) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_command_queue;
  gl::SolidColorPoolFactory solid_color_factory;
  if (dawn_context_provider) {
    d3d11_device = dawn_context_provider->GetD3D11Device();
    d3d12_command_queue = dawn_context_provider->GetD3D12CommandQueue();
#if BUILDFLAG(SKIA_USE_DAWN)
    // When Skia is on Graphite-D3D12, use Dawn (the same `wgpu::Device`
    // and `ID3D12CommandQueue` Skia is using) to fill solid-color
    // overlays.
    if (d3d12_command_queue &&
        base::FeatureList::IsEnabled(features::kDCompOnD3D12)) {
      solid_color_factory = CreateDawnTextureSolidColorPoolFactory(
          dawn_context_provider->GetDevice(), d3d12_command_queue);
    }
#endif  // BUILDFLAG(SKIA_USE_DAWN)
  } else {
    d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
  }
  if (!solid_color_factory) {
    // Use DComp surfaces if Dawn or DComp Textures are not available.
    solid_color_factory =
        gl::CreateDCSurfaceSolidColorPoolFactory(d3d11_device);
  }
  gl::InitializeDirectComposition(std::move(d3d11_device),
                                  std::move(d3d12_command_queue),
                                  std::move(solid_color_factory));
}

void InitializeOverlaySettings(GPUInfo* gpu_info,
                               const GpuFeatureInfo& gpu_feature_info) {
  // This has to be called after a context is created, active GPU is identified,
  // and GPU driver bug workarounds are computed again. Otherwise the workaround
  // `disable_direct_composition_video_overlays` may not be correctly applied.
  // Also, this has to be called after falling back to SwiftShader decision is
  // finalized because this function depends on GL is ANGLE's GLES or not.
  gl::DirectCompositionOverlayWorkarounds workarounds = {
      .disable_sw_video_overlays = gpu_feature_info.IsWorkaroundEnabled(
          DISABLE_DIRECT_COMPOSITION_SW_VIDEO_OVERLAYS),
      .disable_decode_swap_chain =
          gpu_feature_info.IsWorkaroundEnabled(DISABLE_DECODE_SWAP_CHAIN),
      .enable_bgra8_overlays_with_yuv_overlay_support =
          gpu_feature_info.IsWorkaroundEnabled(
              gpu::ENABLE_BGRA8_OVERLAYS_WITH_YUV_OVERLAY_SUPPORT),
      .force_nv12_overlay_support =
          gpu_feature_info.IsWorkaroundEnabled(gpu::FORCE_NV12_OVERLAY_SUPPORT),
      .force_rgb10a2_overlay_support = gpu_feature_info.IsWorkaroundEnabled(
          gpu::FORCE_RGB10A2_OVERLAY_SUPPORT),
      .check_ycbcr_studio_g22_left_p709_for_nv12_support =
          gpu_feature_info.IsWorkaroundEnabled(
              gpu::CHECK_YCBCR_STUDIO_G22_LEFT_P709_FOR_NV12_SUPPORT),
      .disable_dcomp_texture =
          gpu_feature_info.IsWorkaroundEnabled(gpu::DISABLE_DCOMP_TEXTURE),
  };
  SetDirectCompositionOverlayWorkarounds(workarounds);

  DCHECK(gpu_info);
  CollectHardwareOverlayInfo(&gpu_info->overlay_info);
}

}  // namespace gpu::gpu_init_internal
