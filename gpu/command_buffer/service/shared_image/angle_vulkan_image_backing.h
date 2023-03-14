// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_H_

#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/scoped_egl_image.h"

namespace gpu {
namespace gles2 {
class TexturePassthrough;
}

class VulkanImage;

class AngleVulkanImageBacking : public ClearTrackingSharedImageBacking,
                                public GLTextureImageRepresentationClient {
 public:
  AngleVulkanImageBacking(const raw_ptr<SharedContextState>& context_state,
                          const Mailbox& mailbox,
                          viz::SharedImageFormat format,
                          const gfx::Size& size,
                          const gfx::ColorSpace& color_space,
                          GrSurfaceOrigin surface_origin,
                          SkAlphaType alpha_type,
                          uint32_t usage);
  ~AngleVulkanImageBacking() override;

  bool Initialize(const base::span<const uint8_t>& data);
  bool InitializeWihGMB(gfx::GpuMemoryBufferHandle handle);

 protected:
  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  // GLTextureImageRepresentationClient implementation.
  bool GLTextureImageRepresentationBeginAccess(bool readonly) override;
  void GLTextureImageRepresentationEndAccess(bool readonly) override;

 private:
  class SkiaAngleVulkanImageRepresentation;

  struct TextureHolderVk {
    TextureHolderVk();
    TextureHolderVk(TextureHolderVk&& other);
    TextureHolderVk& operator=(TextureHolderVk&& other);
    ~TextureHolderVk();

    std::unique_ptr<VulkanImage> vulkan_image;
    GrBackendTexture backend_texture;
    sk_sp<SkPromiseImageTexture> promise_texture;
  };

  struct TextureHolderGL {
    TextureHolderGL();
    TextureHolderGL(TextureHolderGL&& other);
    TextureHolderGL& operator=(TextureHolderGL&& other);
    ~TextureHolderGL();

    gl::ScopedEGLImage egl_image;
    scoped_refptr<gles2::TexturePassthrough> passthrough_texture;
  };

  // The maximum number of GL or Vulkan textures this backing can hold.
  static constexpr size_t kMaxTextures = 3;

  std::vector<sk_sp<SkPromiseImageTexture>> GetPromiseTextures();
  void AcquireTextureANGLE();
  void ReleaseTextureANGLE();
  void PrepareBackendTexture();
  void SyncImageLayoutFromBackendTexture();
  bool BeginAccessSkia(bool readonly);
  void EndAccessSkia();
  bool InitializePassthroughTexture();

  GrDirectContext* gr_context() { return context_state_->gr_context(); }

  const raw_ptr<SharedContextState> context_state_;

  // In general there will be the same number of Vulkan and GL textures.
  // For multi-planar VkFormats there are no equivalent multi-planar GL
  // texture formats so there could be one VkImage and multiple GL textures.
  std::vector<TextureHolderVk> vk_textures_;
  std::vector<TextureHolderGL> gl_textures_;

  // Array of GL texture IDs and layouts to use when acquiring/releasing
  // textures with ANGLE. `gl_layouts_` will be set from VkImage layouts even
  // if GL textures have not been allocated yet.
  std::array<uint32_t, kMaxTextures> gl_texture_ids_{0, 0, 0};
  std::array<GLenum, kMaxTextures> gl_layouts_{GL_NONE, GL_NONE, GL_NONE};

  int surface_msaa_count_ = 0;
  bool is_skia_write_in_process_ = false;
  bool is_gl_write_in_process_ = false;
  int skia_reads_in_process_ = 0;
  int gl_reads_in_process_ = 0;
  bool need_gl_finish_before_destroy_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANGLE_VULKAN_IMAGE_BACKING_H_
