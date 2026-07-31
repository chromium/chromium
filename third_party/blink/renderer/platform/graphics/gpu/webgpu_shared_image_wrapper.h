// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_H_

#include <memory>

#include "base/byte_size.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/paint/paint_image.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace gfx {
class ColorSpace;
class Size;
}  // namespace gfx

namespace blink {

class PLATFORM_EXPORT WebGpuSharedImageWrapper final {
 public:
  static std::unique_ptr<WebGpuSharedImageWrapper> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space);
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
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_for_external_draws_;
  const scoped_refptr<gpu::ClientSharedImage> shared_image_;
  gpu::SyncToken acquire_sync_token_;
  gpu::SyncToken release_sync_token_;
  bool is_cleared_ = false;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;

 private:
  WebGpuSharedImageWrapper(gfx::Size,
                           viz::SharedImageFormat,
                           SkAlphaType,
                           const gfx::ColorSpace&,
                           base::WeakPtr<WebGraphicsContext3DProviderWrapper>);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_H_
