// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_IMAGE_BACKING_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {

// Test implementation of a gles2::Texture backed backing.
class TestImageBacking : public SharedImageBacking {
 public:
  // Constructor which uses a dummy GL texture ID for the backing.
  TestImageBacking(const Mailbox& mailbox,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   const gfx::ColorSpace& color_space,
                   GrSurfaceOrigin surface_origin,
                   SkAlphaType alpha_type,
                   SharedImageUsageSet usage,
                   size_t estimated_size);
  // Constructor which uses a provided GL texture ID for the backing.
  TestImageBacking(const Mailbox& mailbox,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   const gfx::ColorSpace& color_space,
                   GrSurfaceOrigin surface_origin,
                   SkAlphaType alpha_type,
                   SharedImageUsageSet usage,
                   size_t estimated_size,
                   GLuint texture_id);
  ~TestImageBacking() override;

  bool GetUploadFromMemoryCalledAndReset();
  bool GetReadbackToMemoryCalledAndReset();
  using PurgeableCallback = base::RepeatingCallback<void(const gpu::Mailbox&)>;
  void SetPurgeableCallbacks(
      const PurgeableCallback& set_purgeable_callback,
      const PurgeableCallback& set_not_purgeable_callback) {
    set_purgeable_callback_ = set_purgeable_callback;
    set_not_purgeable_callback_ = set_not_purgeable_callback;
  }

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void SetPurgeable(bool purgeable) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {}
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmap) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;

  // Helper functions
  // Return the service ID of the first texture in the backing.
  GLuint service_id() const { return textures_[0]->service_id(); }
  void set_can_access(bool can_access) { can_access_ = can_access; }
  bool can_access() const { return can_access_; }

#if BUILDFLAG(IS_APPLE)
  void set_in_use_by_window_server(bool in_use_by_window_server) {
    in_use_by_window_server_ = in_use_by_window_server;
  }
  bool in_use_by_window_server() const { return in_use_by_window_server_; }
#endif  // BUILDFLAG(IS_APPLE)

 protected:
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  // ProduceSkiaGanesh creates a representation that is backed by |textures_|,
  // which allows for the creation of SkImages from the representation.
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  // ProduceDawn/Overlay all create dummy representations that
  // don't link up to a real texture.
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  std::vector<raw_ptr<gles2::Texture>> textures_;
  std::vector<scoped_refptr<gles2::TexturePassthrough>> passthrough_textures_;

  bool can_access_ = true;
#if BUILDFLAG(IS_APPLE)
  bool in_use_by_window_server_ = false;
#endif

  bool upload_from_memory_called_ = false;
  bool readback_to_memory_called_ = true;
  PurgeableCallback set_purgeable_callback_;
  PurgeableCallback set_not_purgeable_callback_;
};

class TestOverlayImageRepresentation : public OverlayImageRepresentation {
 public:
  TestOverlayImageRepresentation(SharedImageManager* manager,
                                 SharedImageBacking* backing,
                                 MemoryTypeTracker* tracker)
      : OverlayImageRepresentation(manager, backing, tracker) {}

  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBufferFenceSync() override;
#endif

#if BUILDFLAG(IS_APPLE)
  void MarkBackingInUse(bool in_use) {
    static_cast<TestImageBacking*>(backing())->set_in_use_by_window_server(
        in_use);
  }

 private:
  bool IsInUseByWindowServer() const override;
#endif  // BUILDFLAG(IS_APPLE)
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_IMAGE_BACKING_H_
