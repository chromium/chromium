// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEST_SHARED_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEST_SHARED_IMAGE_BACKING_H_

#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {

// Test implementation of a gles2::Texture backed backing.
class TestSharedImageBacking : public SharedImageBacking {
 public:
  // Constructor which uses a dummy GL texture ID for the backing.
  TestSharedImageBacking(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         size_t estimated_size);
  // Constructor which uses a provided GL texture ID for the backing.
  TestSharedImageBacking(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         size_t estimated_size,
                         GLuint texture_id);
  ~TestSharedImageBacking() override;

  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {}
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {}

  // Helper functions
  GLuint service_id() const { return service_id_; }
  void set_can_access(bool can_access) { can_access_ = can_access; }
  bool can_access() const { return can_access_; }

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  // ProduceSkia/Dawn/Overlay all create dummy representations that
  // don't link up to a real texture.
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) override;
  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  const GLuint service_id_ = 0;
  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  bool can_access_ = true;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEST_SHARED_IMAGE_BACKING_H_
