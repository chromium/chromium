// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_REPRESENTATION_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu_cpp.h>

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan_core.h>

namespace gpu {
class VulkanDeviceQueue;
class VulkanImage;
class VulkanImplementation;
}  // namespace gpu
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/gl/dc_layer_overlay_image.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "ui/gfx/mac/io_surface.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"

extern "C" typedef struct AHardwareBuffer AHardwareBuffer;
#endif

#if BUILDFLAG(IS_WIN)
#include <d3d11.h>
#include <wrl/client.h>
#endif

typedef unsigned int GLenum;
namespace skgpu {
class MutableTextureState;
}

namespace cc {
class PaintOpBuffer;
}  // namespace cc

namespace gfx {
class NativePixmap;
}  // namespace gfx

namespace gpu {
class TextureBase;

namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2

enum class RepresentationAccessMode {
  kNone,
  kRead,
  kWrite,
};

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentation

// A representation of a SharedImageBacking for use with a specific use case /
// api.
class GPU_GLES2_EXPORT SharedImageRepresentation {
 public:
  using AccessMode = RepresentationAccessMode;

  // Used by derived classes.
  enum class AllowUnclearedAccess { kYes, kNo };

  SharedImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker);
  virtual ~SharedImageRepresentation();

  viz::SharedImageFormat format() const { return backing_->format(); }
  const gfx::Size& size() const { return backing_->size(); }
  const gfx::ColorSpace& color_space() const { return backing_->color_space(); }
  GrSurfaceOrigin surface_origin() const { return backing_->surface_origin(); }
  SkAlphaType alpha_type() const { return backing_->alpha_type(); }
  SharedImageUsageSet usage() const { return backing_->usage(); }
  const gpu::Mailbox& mailbox() const { return backing_->mailbox(); }
  const std::string& debug_label() const { return backing_->debug_label(); }
  const char* backing_name() const { return backing_->GetName(); }
  MemoryTypeTracker* tracker() { return tracker_; }
  bool IsCleared() const { return backing_->IsCleared(); }
  void SetCleared() { backing_->SetCleared(); }
  gfx::Rect ClearedRect() const { return backing_->ClearedRect(); }
  void SetClearedRect(const gfx::Rect& cleared_rect) {
    backing_->SetClearedRect(cleared_rect);
  }

  // Indicates that the underlying graphics context has been lost, and the
  // backing should be treated as destroyed.
  void OnContextLost() {
    has_context_ = false;
    backing_->OnContextLost();
  }

  // Returns the number of image planes expected based on the backing format.
  size_t NumPlanesExpected() const;

 protected:
  SharedImageManager* manager() const { return manager_; }
  SharedImageBacking* backing() const { return backing_; }
  bool has_context() const { return has_context_; }

  // Helper class for derived classes' Scoped*Access objects. Has tracking to
  // ensure a Scoped*Access does not outlive the representation it's associated
  // with.
  template <typename RepresentationClass>
  class ScopedAccessBase {
   public:
    ScopedAccessBase(RepresentationClass* representation,
                     AccessMode access_mode)
        : representation_(representation) {
      CHECK_EQ(representation_->access_mode_, AccessMode::kNone);
      representation_->access_mode_ = access_mode;
    }

    ScopedAccessBase(const ScopedAccessBase&) = delete;
    ScopedAccessBase& operator=(const ScopedAccessBase&) = delete;

    ~ScopedAccessBase() {
      CHECK_NE(representation_->access_mode_, AccessMode::kNone);
      representation_->access_mode_ = AccessMode::kNone;
    }

    RepresentationClass* representation() { return representation_; }
    const RepresentationClass* representation() const {
      return representation_;
    }

   private:
    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
    RAW_PTR_EXCLUSION RepresentationClass* const representation_ = nullptr;
  };

 private:
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
  RAW_PTR_EXCLUSION SharedImageManager* const manager_ = nullptr;
  RAW_PTR_EXCLUSION SharedImageBacking* backing_ = nullptr;
  RAW_PTR_EXCLUSION MemoryTypeTracker* const tracker_ = nullptr;
  bool has_context_ = true;
  AccessMode access_mode_ = AccessMode::kNone;
};

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationFactoryRef
class SharedImageRepresentationFactoryRef : public SharedImageRepresentation {
 public:
  SharedImageRepresentationFactoryRef(SharedImageManager* manager,
                                      SharedImageBacking* backing,
                                      MemoryTypeTracker* tracker,
                                      bool is_primary);

  ~SharedImageRepresentationFactoryRef() override;

  const Mailbox& mailbox() const { return backing()->mailbox(); }
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) {
    backing()->Update(std::move(in_fence));
    backing()->OnWriteSucceeded();
  }
  void SetPurgeable(bool purgeable) { backing()->SetPurgeable(purgeable); }
  bool CopyToGpuMemoryBuffer() { return backing()->CopyToGpuMemoryBuffer(); }
  void CopyToGpuMemoryBufferAsync(base::OnceCallback<void(bool)> callback) {
    backing()->CopyToGpuMemoryBufferAsync(std::move(callback));
  }
  void GetGpuMemoryBufferHandleInfo(gfx::GpuMemoryBufferHandle& handle,
                                    viz::SharedImageFormat& format,
                                    gfx::Size& size,
                                    gfx::BufferUsage& buffer_usage) {
    handle = backing()->GetGpuMemoryBufferHandle();
    format = backing()->format();
    size = backing()->size();
    buffer_usage = backing()->buffer_usage();
  }
  bool PresentSwapChain() { return backing()->PresentSwapChain(); }
  void RegisterImageFactory(SharedImageFactory* factory) {
    DCHECK(is_primary_);
    backing()->RegisterImageFactory(factory);
  }

 private:
  const bool is_primary_;
};

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageRepresentationBase

class GPU_GLES2_EXPORT GLTextureImageRepresentationBase
    : public SharedImageRepresentation {
 public:
  static constexpr GLenum kReadAccessMode = 0x8AF6;

  class ScopedAccess
      : public ScopedAccessBase<GLTextureImageRepresentationBase> {
   public:
    ScopedAccess(base::PassKey<GLTextureImageRepresentationBase> pass_key,
                 GLTextureImageRepresentationBase* representation,
                 AccessMode access_mode)
        : ScopedAccessBase(representation, access_mode) {}
    ~ScopedAccess() {
      representation()->UpdateClearedStateOnEndAccess();
      representation()->EndAccess();
    }
  };

  GLTextureImageRepresentationBase(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      GLenum mode,
      AllowUnclearedAccess allow_uncleared);

  // Gets the texture associated with the `plane_index` for SharedImageFormat.
  virtual gpu::TextureBase* GetTextureBase(int plane_index) = 0;
  // Calls GetTextureBase with `plane_index` = 0 for single planar formats eg.
  // RGB.
  gpu::TextureBase* GetTextureBase();

 protected:
  friend class SkiaGLImageRepresentation;
  friend class DawnEGLImageRepresentation;
  friend class DawnGLTextureRepresentation;
  friend class GLTextureGLCommonRepresentation;

  // Can be overridden to handle clear state tracking when GL access begins or
  // ends.
  virtual void UpdateClearedStateOnBeginAccess() {}
  virtual void UpdateClearedStateOnEndAccess() {}

  virtual bool BeginAccess(GLenum mode) = 0;
  virtual void EndAccess() = 0;

  virtual bool SupportsMultipleConcurrentReadAccess();
};

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageRepresentation

class GPU_GLES2_EXPORT GLTextureImageRepresentation
    : public GLTextureImageRepresentationBase {
 public:
  GLTextureImageRepresentation(SharedImageManager* manager,
                               SharedImageBacking* backing,
                               MemoryTypeTracker* tracker)
      : GLTextureImageRepresentationBase(manager, backing, tracker) {}

  // Gets the texture associated with the `plane_index` for SharedImageFormat.
  virtual gles2::Texture* GetTexture(int plane_index) = 0;
  // Calls GetTexture with `plane_index` = 0 for single planar formats eg. RGB.
  gles2::Texture* GetTexture();

  gpu::TextureBase* GetTextureBase(int plane_index) override;

 protected:
  friend class WrappedGLTextureCompoundImageRepresentation;

  void UpdateClearedStateOnBeginAccess() override;
  void UpdateClearedStateOnEndAccess() override;
};

///////////////////////////////////////////////////////////////////////////////
// GLTexturePassthroughImageRepresentation

class GPU_GLES2_EXPORT GLTexturePassthroughImageRepresentation
    : public GLTextureImageRepresentationBase {
 public:
  GLTexturePassthroughImageRepresentation(SharedImageManager* manager,
                                          SharedImageBacking* backing,
                                          MemoryTypeTracker* tracker)
      : GLTextureImageRepresentationBase(manager, backing, tracker) {}

  // Gets the passthrough texture associated with the `plane_index` for
  // SharedImageFormat.
  virtual const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) = 0;
  // Calls GetTexturePassthrough with `plane_index` = 0 for single planar
  // formats eg. RGB.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough();

  gpu::TextureBase* GetTextureBase(int plane_index) override;

  // Returns true if access must be suspended in between GL decoder tasks due to
  // DXGI keyed mutex. Only implemented for D3D GL representation.
  virtual bool NeedsSuspendAccessForDXGIKeyedMutex() const;

 private:
  friend class WrappedGLTexturePassthroughCompoundImageRepresentation;
};

///////////////////////////////////////////////////////////////////////////////
// SkiaImageRepresentation

class GPU_GLES2_EXPORT SkiaImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<SkiaImageRepresentation> {
   public:
    virtual ~ScopedWriteAccess();

    // NOTE: All references to the returned SkSurface(s) must be destroyed
    // before ScopedWriteAccess is destroyed.
    bool has_surfaces() const { return !surfaces_.empty(); }
    SkSurface* surface() const {
      // Writes do not support external sampler.
      CHECK(representation()->format().is_single_plane());
      return surface(0);
    }
    SkSurface* surface(int plane_index) const {
      return surfaces_[plane_index].get();
    }

    GrPromiseImageTexture* promise_image_texture(int plane_index) const {
      return promise_image_textures_[plane_index].get();
    }

    skgpu::graphite::BackendTexture graphite_texture(int plane_index) const {
      return graphite_textures_[plane_index];
    }

    // Return the representations's implementation.
    bool NeedGraphiteContextSubmit();

    // NOTE: Implemented only for Ganesh.
    // Checks if there's a need to apply skgpu::MutableTextureState.
    virtual bool HasBackendSurfaceEndState() = 0;
    // NOTE: Implemented only for Ganesh.
    // Applies the skgpu::MutableTextureState for Vulkan layout and external
    // queue transitions needed for Vulkan/GL interop.
    virtual void ApplyBackendSurfaceEndState() = 0;

   protected:
    ScopedWriteAccess(SkiaImageRepresentation* representation,
                      std::vector<sk_sp<SkSurface>> surfaces);
    ScopedWriteAccess(
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures);
    ScopedWriteAccess(
        SkiaImageRepresentation* representation,
        std::vector<skgpu::graphite::BackendTexture> graphite_textures);

    // A vector of surfaces, promise textures and graphite backend textures
    // corresponding to the number of planes in SharedImageFormat.
    std::vector<sk_sp<SkSurface>> surfaces_;
    // NOTE: Used only for Ganesh.
    std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures_;
    // NOTE: Used only for Graphite.
    std::vector<skgpu::graphite::BackendTexture> graphite_textures_;
  };

  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<SkiaImageRepresentation> {
   public:
    virtual ~ScopedReadAccess();

    GrPromiseImageTexture* promise_image_texture() const {
      CHECK_EQ(representation()->NumPlanesExpected(), 1u);
      return promise_image_texture(0);
    }
    GrPromiseImageTexture* promise_image_texture(int plane_index) const {
      return promise_image_textures_[plane_index].get();
    }

    skgpu::graphite::BackendTexture graphite_texture() const {
      CHECK_EQ(representation()->NumPlanesExpected(), 1u);
      return graphite_texture(0);
    }
    skgpu::graphite::BackendTexture graphite_texture(int plane_index) const {
      return graphite_textures_[plane_index];
    }

    // Return the representations's implementation.
    bool NeedGraphiteContextSubmit();

    // Creates an SkImage from BackendTexture for single planar formats or if
    // format prefers external sampler. Creates an SkImage from
    // YUVABackendTexture for multiplanar formats.
    virtual sk_sp<SkImage> CreateSkImage(
        SharedContextState* context_state,
        SkImages::TextureReleaseProc texture_release_proc = nullptr,
        SkImages::ReleaseContext release_context = nullptr) = 0;
    // Creates an SkImage for the given `plane_index` for
    // multiplanar formats.
    virtual sk_sp<SkImage> CreateSkImageForPlane(
        int plane_index,
        SharedContextState* context_state,
        SkImages::TextureReleaseProc texture_release_proc = nullptr,
        SkImages::ReleaseContext release_context = nullptr) = 0;

    // NOTE: Implemented only for Ganesh.
    // Checks if there's a need to apply skgpu::MutableTextureState.
    virtual bool HasBackendSurfaceEndState() = 0;
    // Applies the skgpu::MutableTextureState for Vulkan layout and external
    // queue transitions needed for Vulkan/GL interop.
    virtual void ApplyBackendSurfaceEndState() = 0;

   protected:
    ScopedReadAccess(
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures);
    ScopedReadAccess(
        SkiaImageRepresentation* representation,
        std::vector<skgpu::graphite::BackendTexture> graphite_textures);

    // A vector of promise textures and graphite backend textures corresponding
    // to the number of planes in SharedImageFormat. NOTE: Used only for Ganesh.
    std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures_;
    // NOTE: Used only for Graphite.
    std::vector<skgpu::graphite::BackendTexture> graphite_textures_;
  };

  SkiaImageRepresentation(SharedImageManager* manager,
                          SharedImageBacking* backing,
                          MemoryTypeTracker* tracker);
  ~SkiaImageRepresentation() override;

  // Note: See BeginWriteAccess below for a description of the semaphore
  // parameters.
  virtual std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) = 0;
  virtual std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) = 0;
  virtual std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) = 0;

  // Note: See BeginReadAccess below for a description of the semaphore
  // parameters.
  virtual std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) = 0;

  // NOTE: Implemented only for Graphite.
  virtual bool NeedGraphiteContextSubmitBeforeEndAccess() = 0;

  virtual bool SupportsMultipleConcurrentReadAccess();

 protected:
  virtual void EndWriteAccess() = 0;
  virtual void EndReadAccess() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// SkiaGaneshImageRepresentation

class GPU_GLES2_EXPORT SkiaGaneshImageRepresentation
    : public SkiaImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedGaneshWriteAccess : public ScopedWriteAccess {
   public:
    ScopedGaneshWriteAccess(
        base::PassKey<SkiaGaneshImageRepresentation> pass_key,
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<SkSurface>> surfaces,
        std::unique_ptr<skgpu::MutableTextureState> end_state);
    ScopedGaneshWriteAccess(
        base::PassKey<SkiaGaneshImageRepresentation> pass_key,
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures,
        std::unique_ptr<skgpu::MutableTextureState> end_state);
    ~ScopedGaneshWriteAccess() override;

    // Checks if there's a need to apply skgpu::MutableTextureState.
    bool HasBackendSurfaceEndState() override;
    // Applies the skgpu::MutableTextureState for Vulkan layout and external
    // queue transitions needed for Vulkan/GL interop.
    void ApplyBackendSurfaceEndState() override;

   private:
    SkiaGaneshImageRepresentation* ganesh_representation() {
      return static_cast<SkiaGaneshImageRepresentation*>(representation());
    }

    std::unique_ptr<skgpu::MutableTextureState> end_state_;
  };

  class GPU_GLES2_EXPORT ScopedGaneshReadAccess : public ScopedReadAccess {
   public:
    ScopedGaneshReadAccess(
        base::PassKey<SkiaGaneshImageRepresentation> pass_key,
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures,
        std::unique_ptr<skgpu::MutableTextureState> end_state);
    ~ScopedGaneshReadAccess() override;

    // Creates an SkImage from GrBackendTexture for single planar formats or if
    // format prefers external sampler. Creates an SkImage from
    // GrYUVABackendTexture for multiplanar formats.
    sk_sp<SkImage> CreateSkImage(
        SharedContextState* context_state,
        SkImages::TextureReleaseProc texture_release_proc = nullptr,
        SkImages::ReleaseContext release_context = nullptr) override;
    // Creates an SkImage for the given `plane_index` from GrBackendTexture for
    // multiplanar formats.
    sk_sp<SkImage> CreateSkImageForPlane(
        int plane_index,
        SharedContextState* context_state,
        SkImages::TextureReleaseProc texture_release_proc = nullptr,
        SkImages::ReleaseContext release_context = nullptr) override;

    // Checks if there's a need to apply skgpu::MutableTextureState.
    bool HasBackendSurfaceEndState() override;
    // Applies the skgpu::MutableTextureState for Vulkan layout and external
    // queue transitions needed for Vulkan/GL interop.
    void ApplyBackendSurfaceEndState() override;

   private:
    SkiaGaneshImageRepresentation* ganesh_representation() {
      return static_cast<SkiaGaneshImageRepresentation*>(representation());
    }

    std::unique_ptr<skgpu::MutableTextureState> end_state_;
  };

  SkiaGaneshImageRepresentation(GrDirectContext* gr_context,
                                SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker);

  GrDirectContext* gr_context() const { return gr_context_; }

  // Note: See BeginWriteAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) override;

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) override;

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) override;

  // Note: See BeginReadAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;

  // Return false for ganesh.
  bool NeedGraphiteContextSubmitBeforeEndAccess() final;

 protected:
  friend class WrappedSkiaGaneshCompoundImageRepresentation;

  // Begin the write access.
  //
  // update_rect is a hint to the backend about the portion of the image that
  // will be drawn to. Callers shouldn't draw outside of this area, but aren't
  // required to overwrite every pixel inside it.
  //
  // The implementations should insert semaphores into begin_semaphores vector
  // which client will wait on before writing the backing. The ownership of
  // begin_semaphores is not passed to client. The implementations can also
  // optionally insert semaphores into end_semaphores. If using end_semaphores,
  // the client must submit them with drawing operations which use the backing.
  // The ownership of end_semaphores are not passed to client. And client must
  // submit the end_semaphores before calling EndWriteAccess().
  //
  // The backing can assign end_state, and the caller must reset backing's state
  // to the end_state before calling EndWriteAccess().
  // Returns an empty vector on failure.
  virtual std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) = 0;
  virtual std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) = 0;

  // Begin the read access. The implementations should insert semaphores into
  // begin_semaphores vector which client will wait on before reading the
  // backing. The ownership of begin_semaphores is not passed to client.
  // The implementations can also optionally insert semaphores into
  // end_semaphores. If using end_semaphores, the client must submit them with
  // drawing operations which use the backing. The ownership of end_semaphores
  // are not passed to client. And client must submit the end_semaphores before
  // calling EndReadAccess().
  // The backing can assign end_state, and the caller must reset backing's state
  // to the end_state before calling EndReadAccess().
  // Returns an empty vector on failure.
  virtual std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) = 0;

 private:
  raw_ptr<GrDirectContext> gr_context_ = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// SkiaGraphiteImageRepresentation

class GPU_GLES2_EXPORT SkiaGraphiteImageRepresentation
    : public SkiaImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedGraphiteWriteAccess : public ScopedWriteAccess {
   public:
    ScopedGraphiteWriteAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> pass_key,
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<SkSurface>> surfaces);
    ScopedGraphiteWriteAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> pass_key,
        SkiaImageRepresentation* representation,
        std::vector<skgpu::graphite::BackendTexture> graphite_textures);
    ~ScopedGraphiteWriteAccess() override;

    bool HasBackendSurfaceEndState() override;
    void ApplyBackendSurfaceEndState() override;
  };

  class GPU_GLES2_EXPORT ScopedGraphiteReadAccess : public ScopedReadAccess {
   public:
    ScopedGraphiteReadAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> pass_key,
        SkiaImageRepresentation* representation,
        std::vector<skgpu::graphite::BackendTexture> graphite_textures);
    ~ScopedGraphiteReadAccess() override;

    // Creates an SkImage from BackendTexture for single planar formats or if
    // format prefers external sampler. Creates an SkImage from
    // YUVABackendTexture for multiplanar formats.
    sk_sp<SkImage> CreateSkImage(
        SharedContextState* context_state,
        SkImages::TextureReleaseProc texture_release_proc = nullptr,
        SkImages::ReleaseContext release_context = nullptr) override;
    // Creates an SkImage for the given `plane_index` from BackendTexture for
    // multiplanar formats.
    sk_sp<SkImage> CreateSkImageForPlane(
        int plane_index,
        SharedContextState* context_state,
        SkImages::TextureReleaseProc texture_release_proc = nullptr,
        SkImages::ReleaseContext release_context = nullptr) override;

    bool HasBackendSurfaceEndState() override;
    void ApplyBackendSurfaceEndState() override;
  };

  SkiaGraphiteImageRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  MemoryTypeTracker* tracker);

  // Note: See BeginWriteAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) override;

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) override;

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true) override;

  // Note: See BeginReadAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;

  bool NeedGraphiteContextSubmitBeforeEndAccess() override;

 protected:
  friend class WrappedSkiaGraphiteCompoundImageRepresentation;

  std::string WrappedTextureDebugLabel(int plane) const;

  // Begin the write access. Returns an empty vector on failure.
  //
  // update_rect is a hint to the backend about the portion of the image that
  // will be drawn to. Callers shouldn't draw outside of this area, but aren't
  // required to overwrite every pixel inside it.
  virtual std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect) = 0;
  virtual std::vector<skgpu::graphite::BackendTexture> BeginWriteAccess() = 0;

  // Returns an empty vector on failure.
  virtual std::vector<skgpu::graphite::BackendTexture> BeginReadAccess() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// DawnImageRepresentation

class GPU_GLES2_EXPORT DawnImageRepresentation
    : public SharedImageRepresentation {
 public:
  static constexpr wgpu::TextureUsage kWriteUsage =
      wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::RenderAttachment |
      wgpu::TextureUsage::StorageBinding;

  DawnImageRepresentation(SharedImageManager* manager,
                          SharedImageBacking* backing,
                          MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class GPU_GLES2_EXPORT ScopedAccess
      : public ScopedAccessBase<DawnImageRepresentation> {
   public:
    ScopedAccess(base::PassKey<DawnImageRepresentation> pass_key,
                 DawnImageRepresentation* representation,
                 wgpu::Texture texture,
                 AccessMode access_mode);
    ~ScopedAccess();

    // Get the unowned texture handle. The caller should take a reference
    // if necessary by doing wgpu::Texture texture(access->texture());
    const wgpu::Texture& texture() const { return texture_; }

   private:
    wgpu::Texture texture_;
  };

  // Calls BeginAccess and returns a ScopedAccess object which will EndAccess
  // when it goes out of scope. The Representation must outlive the returned
  // ScopedAccess.
  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      wgpu::TextureUsage usage,
      AllowUnclearedAccess allow_uncleared);

  // Allawos passing internal usages to the created Dawn texture.
  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      wgpu::TextureUsage usage,
      wgpu::TextureUsage internal_usage,
      AllowUnclearedAccess allow_uncleared);

  // For write usage, the update_rect is a hint to the backend about the portion
  // of the image that will be drawn to. Callers shouldn't draw outside of this
  // area, but aren't required to overwrite every pixel inside it.
  // For non-write usage, the update_rect can be ignored.
  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      wgpu::TextureUsage usage,
      AllowUnclearedAccess allow_uncleared,
      const gfx::Rect& update_rect);

  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      wgpu::TextureUsage usage,
      wgpu::TextureUsage internal_usage,
      AllowUnclearedAccess allow_uncleared,
      const gfx::Rect& update_rect);

  virtual bool SupportsMultipleConcurrentReadAccess();

 private:
  friend class WrappedDawnCompoundImageRepresentation;

  // This can return null in case of a Dawn validation error, for example if
  // usage is invalid.
  virtual wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                                    wgpu::TextureUsage internal_usage) = 0;
  virtual wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                                    wgpu::TextureUsage internal_usage,
                                    const gfx::Rect& update_rect);
  virtual void EndAccess() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// DawnBufferRepresentation

class GPU_GLES2_EXPORT DawnBufferRepresentation
    : public SharedImageRepresentation {
 public:
  DawnBufferRepresentation(SharedImageManager* manager,
                           SharedImageBacking* backing,
                           MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class GPU_GLES2_EXPORT ScopedAccess
      : public ScopedAccessBase<DawnBufferRepresentation> {
   public:
    ScopedAccess(base::PassKey<DawnBufferRepresentation> pass_key,
                 DawnBufferRepresentation* representation,
                 wgpu::Buffer buffer,
                 AccessMode access_mode);
    ~ScopedAccess();

    std::unique_ptr<ScopedAccess> BeginScopedAccess(wgpu::BufferUsage usage);

    const wgpu::Buffer& buffer() const { return buffer_; }

   private:
    wgpu::Buffer buffer_;
  };

  // Allows passing usages to the created Dawn buffer.
  std::unique_ptr<ScopedAccess> BeginScopedAccess(wgpu::BufferUsage usage);

 private:
  virtual wgpu::Buffer BeginAccess(wgpu::BufferUsage usage) = 0;
  virtual void EndAccess() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// OverlayImageRepresentation

class GPU_GLES2_EXPORT OverlayImageRepresentation
    : public SharedImageRepresentation {
 public:
  OverlayImageRepresentation(SharedImageManager* manager,
                             SharedImageBacking* backing,
                             MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<OverlayImageRepresentation> {
   public:
    ScopedReadAccess(base::PassKey<OverlayImageRepresentation> pass_key,
                     OverlayImageRepresentation* representation,
                     gfx::GpuFenceHandle acquire_fence);
    ~ScopedReadAccess();

#if BUILDFLAG(IS_ANDROID)
    AHardwareBuffer* GetAHardwareBuffer() {
      return representation()->GetAHardwareBuffer();
    }
    // Deprecated. All code should use GetAHardwareBuffer() above, this function
    // will be deleted when GLSurfaceEGLSurface control will be able to deliver
    // fences via EndAccess.
    std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
    GetAHardwareBufferFenceSync() {
      return representation()->GetAHardwareBufferFenceSync();
    }
#elif BUILDFLAG(IS_OZONE)
    scoped_refptr<gfx::NativePixmap> GetNativePixmap() {
      return representation()->GetNativePixmap();
    }
#elif BUILDFLAG(IS_WIN)
    std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() {
      return representation()->GetDCLayerOverlayImage();
    }
#elif BUILDFLAG(IS_APPLE)
    gfx::ScopedIOSurface GetIOSurface() const {
      return representation()->GetIOSurface();
    }
    bool IsInUseByWindowServer() const {
      return representation()->IsInUseByWindowServer();
    }
#endif

    gfx::GpuFenceHandle TakeAcquireFence() { return std::move(acquire_fence_); }
    void SetReleaseFence(gfx::GpuFenceHandle release_fence) {
      // Note: We overwrite previous fence. In case if window manager uses fence
      // for each frame we schedule overlay and the same image is scheduled for
      // multiple frames this will be updated after each frame. It's safe to
      // wait only for the last frame's fence.
      release_fence_ = std::move(release_fence);
    }

   private:
    gfx::GpuFenceHandle acquire_fence_;
    gfx::GpuFenceHandle release_fence_;
  };

  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess();

 protected:
  friend class WrappedOverlayCompoundImageRepresentation;

  // Notifies the backing that an access will start. Returns false if there is a
  // conflict. Otherwise, returns true and:
  // - Set a gpu fence to |acquire_fence| that should be waited on before the
  // SharedImage is ready to be displayed. This fence is fired when the gpu
  // has finished writing.
  virtual bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) = 0;

  // |release_fence| is a fence that will be signaled when the image can be
  // safely re-used. Note, on some platforms window manager doesn't support
  // release fences and return image when it's already safe to re-use.
  // |release_fence| will be null in that case.
  virtual void EndReadAccess(gfx::GpuFenceHandle release_fence) = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual AHardwareBuffer* GetAHardwareBuffer();
  virtual std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBufferFenceSync();
#elif BUILDFLAG(IS_OZONE)
  scoped_refptr<gfx::NativePixmap> GetNativePixmap();
#elif BUILDFLAG(IS_WIN)
  virtual std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage();
#elif BUILDFLAG(IS_APPLE)
  virtual gfx::ScopedIOSurface GetIOSurface() const;
  // Return true if the macOS WindowServer is currently using the underlying
  // storage for the image.
  virtual bool IsInUseByWindowServer() const;
#endif
};

///////////////////////////////////////////////////////////////////////////////
// LegacyOverlayImageRepresentation

#if BUILDFLAG(IS_ANDROID)
class GPU_GLES2_EXPORT LegacyOverlayImageRepresentation
    : public SharedImageRepresentation {
 public:
  LegacyOverlayImageRepresentation(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  // Renders shared image to SurfaceView/Dialog overlay. Should only be called
  // if the image already promoted to overlay.
  virtual void RenderToOverlay() = 0;

  // Notifies legacy overlay system about overlay promotion.
  virtual void NotifyOverlayPromotion(bool promotion,
                                      const gfx::Rect& bounds) = 0;
};
#endif

///////////////////////////////////////////////////////////////////////////////
// MemoryImageRepresentation

class GPU_GLES2_EXPORT MemoryImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<MemoryImageRepresentation> {
   public:
    ScopedReadAccess(base::PassKey<MemoryImageRepresentation> pass_key,
                     MemoryImageRepresentation* representation,
                     SkPixmap pixmap);
    ~ScopedReadAccess();

    SkPixmap pixmap() { return pixmap_; }

   private:
    SkPixmap pixmap_;
  };

  MemoryImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess();

 protected:
  virtual SkPixmap BeginReadAccess() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// RasterImageRepresentation

// Representation of a SharedImageBacking for raster work.
// This representation is used for raster work and compositor. The raster work
// will be converted to a cc::PaintOpBuffer and stored in the
// SharedImageBacking. And then the the compositor will access the stored
// cc::PaintOpBuffer and execute paint ops in it.
class GPU_GLES2_EXPORT RasterImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<RasterImageRepresentation> {
   public:
    ScopedReadAccess(base::PassKey<RasterImageRepresentation> pass_key,
                     RasterImageRepresentation* representation,
                     const cc::PaintOpBuffer* paint_op_buffer,
                     const std::optional<SkColor4f>& clear_color);
    ~ScopedReadAccess();

    const cc::PaintOpBuffer* paint_op_buffer() const {
      return paint_op_buffer_;
    }
    const std::optional<SkColor4f>& clear_color() const { return clear_color_; }

   private:
    const raw_ptr<const cc::PaintOpBuffer> paint_op_buffer_;
    std::optional<SkColor4f> clear_color_;
  };

  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<RasterImageRepresentation> {
   public:
    ScopedWriteAccess(base::PassKey<RasterImageRepresentation> pass_key,
                      RasterImageRepresentation* representation,
                      cc::PaintOpBuffer* paint_op_buffer);
    ~ScopedWriteAccess();

    cc::PaintOpBuffer* paint_op_buffer() { return paint_op_buffer_; }
    // An optional callback which will be called when the all paint ops in the
    // |paint_op_buffer_| are released.
    void set_callback(base::OnceClosure callback) {
      DCHECK(!callback_);
      DCHECK(callback);
      callback_ = std::move(callback);
    }

   private:
    const raw_ptr<cc::PaintOpBuffer> paint_op_buffer_;
    base::OnceClosure callback_;
  };

  RasterImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess();

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      scoped_refptr<SharedContextState> context_state,
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const std::optional<SkColor4f>& clear_color,
      bool visible);

 protected:
  virtual cc::PaintOpBuffer* BeginReadAccess(
      std::optional<SkColor4f>& clear_color) = 0;
  virtual void EndReadAccess() = 0;
  virtual cc::PaintOpBuffer* BeginWriteAccess(
      scoped_refptr<SharedContextState> context_state,
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const std::optional<SkColor4f>& clear_color,
      bool visible) = 0;
  virtual void EndWriteAccess(base::OnceClosure callback) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// VideoImageRepresentation

class GPU_GLES2_EXPORT VideoImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<VideoImageRepresentation> {
   public:
    ScopedWriteAccess(base::PassKey<VideoImageRepresentation> pass_key,
                      VideoImageRepresentation* representation);
    ~ScopedWriteAccess();

#if BUILDFLAG(IS_WIN)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11Texture() const {
      return representation()->GetD3D11Texture();
    }
#endif  // BUILDFLAG(IS_WIN)
  };

  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<VideoImageRepresentation> {
   public:
    ScopedReadAccess(base::PassKey<VideoImageRepresentation> pass_key,
                     VideoImageRepresentation* representation);
    ~ScopedReadAccess();

#if BUILDFLAG(IS_WIN)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11Texture() const {
      return representation()->GetD3D11Texture();
    }
#endif  // BUILDFLAG(IS_WIN)
  };

  VideoImageRepresentation(SharedImageManager* manager,
                           SharedImageBacking* backing,
                           MemoryTypeTracker* tracker);
  ~VideoImageRepresentation() override;

  virtual std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess();
  virtual std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess();

 protected:
#if BUILDFLAG(IS_WIN)
  virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11Texture() const = 0;
#endif  // BUILDFLAG(IS_WIN)
  virtual bool BeginWriteAccess() = 0;
  virtual void EndWriteAccess() = 0;
  virtual bool BeginReadAccess() = 0;
  virtual void EndReadAccess() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// VulkanImageRepresentation

#if BUILDFLAG(ENABLE_VULKAN)
class GPU_GLES2_EXPORT VulkanImageRepresentation
    : public SharedImageRepresentation {
 public:
  VulkanImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker,
                            std::unique_ptr<gpu::VulkanImage> vulkan_image,
                            gpu::VulkanDeviceQueue* vulkan_device_queue,
                            gpu::VulkanImplementation& vulkan_impl);
  ~VulkanImageRepresentation() override;

  class ScopedAccess : public ScopedAccessBase<VulkanImageRepresentation> {
   public:
    ScopedAccess(VulkanImageRepresentation* representation,
                 AccessMode access_mode,
                 std::vector<VkSemaphore> begin_semaphores,
                 VkSemaphore end_semaphore);
    ~ScopedAccess();

    gpu::VulkanImage& GetVulkanImage();

   private:
    bool is_read_only_;
    std::vector<VkSemaphore> begin_semaphores_;
    VkSemaphore end_semaphore_;
  };

  virtual std::unique_ptr<ScopedAccess> BeginScopedAccess(
      AccessMode access_mode,
      std::vector<VkSemaphore>& begin_semaphores,
      std::vector<VkSemaphore>& end_semaphores) = 0;

 protected:
  virtual void EndScopedAccess(bool is_read_only,
                               VkSemaphore end_semaphore) = 0;

  std::unique_ptr<gpu::VulkanImage> vulkan_image_;
  raw_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue_;
  raw_ref<VulkanImplementation> vulkan_impl_;
};
#endif

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_REPRESENTATION_H_
