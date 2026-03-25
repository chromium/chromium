// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_CANVAS_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_CANVAS_UTILS_H_

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class WebGraphicsContext3DProviderWrapper;

PLATFORM_EXPORT bool Accelerated2DCanvasFeatureEnabled(
    WebGraphicsContext3DProviderWrapper*);

PLATFORM_EXPORT bool IsScanoutSupportedForCanvasWithFormat(
    viz::SharedImageFormat format,
    const gpu::Capabilities& capabilities);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_CANVAS_UTILS_H_
