// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_CANVAS_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_CANVAS_UTILS_H_

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace gpu {
class SharedImageInterface;
}

namespace blink {

class WebGraphicsContext3DProviderWrapper;

PLATFORM_EXPORT bool Accelerated2DCanvasFeatureEnabled(
    WebGraphicsContext3DProviderWrapper*);

PLATFORM_EXPORT bool IsScanoutSupportedForCanvasWithFormat(
    viz::SharedImageFormat format,
    const gpu::Capabilities& capabilities);

// Whether WebGL content should be placed into overlays.
PLATFORM_EXPORT bool UseOverlaysForWebGL();

// Forces UseOverlaysForWebGL() to return the passed-in value.
PLATFORM_EXPORT void SetUseOverlaysForWebGLForTesting(bool enable);

// Whether mappable SharedImages should be used for canvas2d content with CPU
// raster.
PLATFORM_EXPORT bool UseMappableSharedImagesForCanvas2D();

// Forces UseMappableSharedImagesForCanvas2D() to return the passed-in value.
PLATFORM_EXPORT void SetUseMappableSharedImagesForCanvas2DForTesting(
    bool enable);

// Whether SharedImages used for canvas2D content should be placed into
// overlays.
PLATFORM_EXPORT bool UseOverlaysForCanvas2D();

// Whether SharedImages used for canvas2D content that is rasterized according
// to `raster_mode` may be given usage optimized for low-latency (SCANOUT and
// CONCURRENT_READ_WRITE).
PLATFORM_EXPORT bool LowLatencyUsageSupportedForCanvas2D(
    RasterMode raster_mode);

// Whether SharedImages used for WebGL content may be given usage optimized
// for low-latency (SCANOUT and CONCURRENT_READ_WRITE).
PLATFORM_EXPORT bool LowLatencyUsageSupportedForWebGL(
    gpu::SharedImageInterface*);

// Forces LowLatencyUsageSupportedForWebGL() to return the passed-in value.
PLATFORM_EXPORT void SetLowLatencyUsageSupportedForWebGLForTesting(bool enable);

// Forces LowLatencyUsageSupportedForCanvas2D() to return the
// passed-in value.
PLATFORM_EXPORT void SetLowLatencyUsageSupportedForCanvas2DForTesting(
    bool enable);

class PLATFORM_EXPORT ScopedCanvasUtils {
 public:
  ScopedCanvasUtils() = default;
  ~ScopedCanvasUtils();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_CANVAS_UTILS_H_
