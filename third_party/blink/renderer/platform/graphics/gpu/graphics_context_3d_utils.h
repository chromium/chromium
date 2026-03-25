// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_GRAPHICS_CONTEXT_3D_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_GRAPHICS_CONTEXT_3D_UTILS_H_

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkImage.h"

typedef unsigned int GLenum;

namespace blink {

class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT GraphicsContext3DUtils {
  STATIC_ONLY(GraphicsContext3DUtils);

 public:
  static bool Accelerated2DCanvasFeatureEnabled(
      WebGraphicsContext3DProviderWrapper*);

  // Needs to be static as its caller CreateSharedImageProviderBase is static.
  static bool IsScanoutSupportedForCanvasWithFormat(
      viz::SharedImageFormat format,
      const gpu::Capabilities& capabilities);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_GRAPHICS_CONTEXT_3D_UTILS_H_
