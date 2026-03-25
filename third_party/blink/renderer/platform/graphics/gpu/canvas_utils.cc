// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"

#include "build/build_config.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

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

}  // namespace blink
