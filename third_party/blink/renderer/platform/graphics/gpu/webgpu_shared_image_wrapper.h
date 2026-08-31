// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"

namespace gfx {
class ColorSpace;
class Size;
}  // namespace gfx

namespace blink {

class PLATFORM_EXPORT WebGpuSharedImageWrapper final {
 public:
  WebGpuSharedImageWrapper(scoped_refptr<gpu::ClientSharedImage> shared_image,
                           base::WeakPtr<WebGraphicsContext3DProviderWrapper>
                               context_provider_wrapper);
  ~WebGpuSharedImageWrapper();

  gfx::Size Size() const { return shared_image_->size(); }
  viz::SharedImageFormat GetSharedImageFormat() const {
    return shared_image_->format();
  }
  const gfx::ColorSpace& GetColorSpace() const {
    return shared_image_->color_space();
  }
  SkAlphaType GetAlphaType() const { return shared_image_->alpha_type(); }

  void WaitSyncToken(const gpu::SyncToken& sync_token);

  // Temporarily public for WebGpuSharedImageWrapperLease migration.
  const scoped_refptr<gpu::ClientSharedImage> shared_image_;
  gpu::SyncToken sync_token_;
  bool is_cleared_ = false;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_H_
