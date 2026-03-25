// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

std::optional<bool> g_use_mappable_shared_images_for_canvas_2d_for_testing;
std::optional<bool> g_low_latency_usage_supported_for_canvas_2d_for_testing;
std::optional<bool> g_use_overlays_for_webgl_for_testing;
std::optional<bool> g_low_latency_usage_supported_for_webgl_for_testing;

#if BUILDFLAG(IS_APPLE)
bool IsDelegatedCompositingEnabled() {
  static const bool backed_by_io_surface =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGpuMemoryBufferCompositorResources);
  return backed_by_io_surface;
}
#endif

#if BUILDFLAG(IS_ANDROID)
bool LowLatencyUsageSupportedForCanvas() {
  // Low-latency usage on Android is possible only with SurfaceControl.
  return ::features::IsAndroidSurfaceControlEnabled() &&
         base::FeatureList::IsEnabled(
             features::kLowLatencyUsageSupportedForCanvas);
}
#endif

}  // namespace

bool Accelerated2DCanvasFeatureEnabled(
    WebGraphicsContext3DProviderWrapper* context_provider_wrapper) {
  // Don't use accelerated canvas if compositor is in software mode.
  if (!SharedGpuContext::IsGpuCompositingEnabled()) {
    return false;
  }

  if (!RuntimeEnabledFeatures::Accelerated2dCanvasEnabled()) {
    return false;
  }

  if (!context_provider_wrapper) {
    return false;
  }
  const gpu::GpuFeatureInfo& gpu_feature_info =
      context_provider_wrapper->ContextProvider().GetGpuFeatureInfo();
  return gpu::kGpuFeatureStatusEnabled ==
         gpu_feature_info
             .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS];
}

bool IsScanoutSupportedForCanvasWithFormat(
    viz::SharedImageFormat format,
    const gpu::Capabilities& capabilities) {
  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return true;
  }
  if (format == viz::SinglePlaneFormat::kRGBX_8888) {
    return !capabilities.disable_mac_swangle_rgbx;
  }
  if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return capabilities.texture_format_bgra8888;
  }
  if (format == viz::SinglePlaneFormat::kBGRX_8888) {
    return capabilities.texture_format_bgra8888 &&
           !capabilities.disable_mac_swangle_rgbx;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_F16) {
#if BUILDFLAG(IS_MAC)
    return true;
#else
    return capabilities.texture_half_float_linear;
#endif
  }
  return false;
}

bool UseMappableSharedImagesForCanvas2D() {
  if (g_use_mappable_shared_images_for_canvas_2d_for_testing) {
    return g_use_mappable_shared_images_for_canvas_2d_for_testing.value();
  }

#if BUILDFLAG(IS_APPLE)
  // Native mappable SharedImage is always available on Apple platforms. If
  // delegated compositing is enabled, we exploit this fact to use mappable
  // SharedImages as the backing for 2D canvases. Note that we know that in
  // this case they will have SCANOUT usage added as well (see the method
  // below).
  return IsDelegatedCompositingEnabled();
#else
  return false;
#endif
}

void SetUseMappableSharedImagesForCanvas2DForTesting(bool enable) {
  g_use_mappable_shared_images_for_canvas_2d_for_testing = enable;
}

bool UseOverlaysForCanvas2D() {
#if BUILDFLAG(IS_APPLE)
  // Delegated compositing on Apple platforms is all-or-nothing as there is no
  // API for partial delegation. Hence, if delegated compositing is enabled, we
  // want 2D canvases to end up in overlays.
  // We could consider extending this to other platforms that use delegated
  // compositing (e.g., Windows).
  return IsDelegatedCompositingEnabled();
#else
  // NOTE: On ChromeOS, conceptually this could be gated on the same underlying
  // capability as usage of overlays for WebGL (atomic DRM). However,
  // historically usage of overlays for Canvas2D outside of low-latency was
  // never enabled for ChromeOS.
  return false;
#endif
}

void SetLowLatencyUsageSupportedForCanvas2DForTesting(bool enable) {
  g_low_latency_usage_supported_for_canvas_2d_for_testing = enable;
}

bool LowLatencyUsageSupportedForCanvas2D(RasterMode raster_mode) {
  if (g_low_latency_usage_supported_for_canvas_2d_for_testing) {
    return g_low_latency_usage_supported_for_canvas_2d_for_testing.value();
  }

  // Concurrent read/write only makes sense if raster writes are happening via
  // the GPU.
  if (raster_mode == RasterMode::kCPU) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  // Low-latency usages are supported on Windows if it's possible to back
  // SharedImages by the D3D swapchain.
  return SharedGpuContext::ContextProviderWrapper()
      ->ContextProvider()
      .SharedImageInterface()
      ->GetCapabilities()
      .shared_image_swap_chain;
#elif BUILDFLAG(IS_ANDROID)
  return LowLatencyUsageSupportedForCanvas();
#elif BUILDFLAG(IS_CHROMEOS)
  // Low-latency usage is always supported for Canvas2D on ChromeOS.
  // NOTE: Conceptually this should be gated on the same underlying capability
  // as low-latency WebGL is gated on (atomic DRM). However, historically that
  // gate was never applied for low-latency Canvas2D.
  return true;
#else
  // NOTE: crbug.com/41435781 would need to be resolved in order to support
  // low-latency usage on Mac (currently setting the desynchronized attribute
  // on a canvas is a no-op on Mac). If/once that bug is resolved, determine
  // whether this method can then return true on Apple if
  // IsDelegatedCompositingEnabled() holds.
  return false;
#endif
}

bool LowLatencyUsageSupportedForWebGL(gpu::SharedImageInterface* sii) {
  if (g_low_latency_usage_supported_for_webgl_for_testing) {
    return g_low_latency_usage_supported_for_webgl_for_testing.value();
  }

#if BUILDFLAG(IS_ANDROID)
  return LowLatencyUsageSupportedForCanvas();
#elif BUILDFLAG(IS_CHROMEOS)
  // Whether WebGL canvases should be given low-latency usage is specified on a
  // per-board basis by passing (or not) the relevant command-line flag.
  static const bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      blink::switches::kEnableOverlaysAndLowLatencyUsageForWebGL);
  return enabled;
#elif BUILDFLAG(IS_WIN)
  return sii && sii->GetCapabilities().shared_image_swap_chain;
#else
  // NOTE: crbug.com/41435781 would need to be resolved in order to support
  // low-latency usage on Mac (currently setting the desynchronized attribute
  // on a canvas is a no-op on Mac). If/once that bug is resolved, determine
  // whether this method can then return true on Apple if
  // IsDelegatedCompositingEnabled() holds.
  return false;
#endif
}

bool UseOverlaysForWebGL() {
  if (g_use_overlays_for_webgl_for_testing) {
    return g_use_overlays_for_webgl_for_testing.value();
  }

#if BUILDFLAG(IS_APPLE)
  // Delegated compositing on Apple platforms is all-or-nothing as there is no
  // API for partial delegation. Hence, if delegated compositing is enabled, we
  // want WebGL canvases to end up in overlays.
  // We could consider extending this to other platforms that use delegated
  // compositing (e.g., Windows).
  return IsDelegatedCompositingEnabled();
#elif BUILDFLAG(IS_CHROMEOS)
  // Whether WebGL canvases should be placed in overlays is specified on a
  // per-board basis by passing (or not) the relevant command-line flag.
  static const bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      blink::switches::kEnableOverlaysAndLowLatencyUsageForWebGL);
  return enabled;
#else
  return false;
#endif
}

void SetUseOverlaysForWebGLForTesting(bool enable) {
  g_use_overlays_for_webgl_for_testing = enable;
}

void SetLowLatencyUsageSupportedForWebGLForTesting(bool enable) {
  g_low_latency_usage_supported_for_webgl_for_testing = enable;
}

void ResetCanvasUtilsForTesting() {
  g_use_mappable_shared_images_for_canvas_2d_for_testing.reset();
  g_low_latency_usage_supported_for_canvas_2d_for_testing.reset();
  g_use_overlays_for_webgl_for_testing.reset();
  g_low_latency_usage_supported_for_webgl_for_testing.reset();
}

}  // namespace blink
