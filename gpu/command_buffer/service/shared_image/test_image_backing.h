// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_IMAGE_BACKING_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {

// Test implementation of a gles2::Texture backed backing.
class TestImageBacking : public SharedImageBacking {
 public:
  // Constructor which uses a dummy GL texture ID for the backing.
  TestImageBacking(const Mailbox& mailbox,
                   viz::ResourceFormat format,
                   const gfx::Size& size,
                   const gfx::ColorSpace& color_space,
                   GrSurfaceOrigin surface_origin,
                   SkAlphaType alpha_type,
                   uint32_t usage,
                   size_t estimated_size);
  // Constructor which uses a provided GL texture ID for the backing.
  TestImageBacking(const Mailbox& mailbox,
                   viz::ResourceFormat format,
                   const gfx::Size& size,
                   const gfx::ColorSpace& color_space,
                   GrSurfaceOrigin surface_origin,
                   SkAlphaType alpha_type,
                   uint32_t usage,
                   size_t estimated_size,
                   GLuint texture_id);
  ~TestImageBacking() override;

  bool GetUploadFromMemoryCalledAndReset();
  bool GetReadbackToMemoryCalledAndReset();

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {}
  bool UploadFromMemory(const SkPixmap& pixmap) override;
  bool ReadbackToMemory(SkPixmap& pixmap) override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDumpGuid client_guid,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {}

  // Helper functions
  GLuint service_id() const { return service_id_; }
  void set_can_access(bool can_access) { can_access_ = can_access; }
  bool can_access() const { return can_access_; }

 protected:
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  // ProduceSkia creates a representation that is backed by |texture_|, which
  // allows for the creation of SkImages from the representation.
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  // ProduceDawn/Overlay all create dummy representations that
  // don't link up to a real texture.
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type) override;
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  const GLuint service_id_ = 0;
  raw_ptr<gles2::Texture> texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  bool can_access_ = true;

  bool upload_from_memory_called_ = false;
  bool readback_to_memory_called_ = true;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_IMAGE_BACKING_H_
