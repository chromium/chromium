// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/graphics_context_3d_utils.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool GraphicsContext3DUtils::Accelerated2DCanvasFeatureEnabled() {
  // Don't use accelerated canvas if compositor is in software mode.
  if (!SharedGpuContext::IsGpuCompositingEnabled())
    return false;

  if (!RuntimeEnabledFeatures::Accelerated2dCanvasEnabled())
    return false;

  DCHECK(context_provider_wrapper_);
  const gpu::GpuFeatureInfo& gpu_feature_info =
      context_provider_wrapper_->ContextProvider()->GetGpuFeatureInfo();
  return gpu::kGpuFeatureStatusEnabled ==
         gpu_feature_info
             .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS];
}

}  // namespace blink
