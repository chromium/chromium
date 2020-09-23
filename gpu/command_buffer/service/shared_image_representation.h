// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>
#include <memory>

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurfaceMutableState.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"

typedef unsigned int GLenum;
class SkPromiseImageTexture;

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace gl {
class GLImage;
}

namespace gfx {
class NativePixmap;
}  // namespace gfx

namespace media {
class VASurface;
}  // namespace media

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

// A representation of a SharedImageBacking for use with a specific use case /
// api.
class GPU_GLES2_EXPORT SharedImageRepresentation {
 public:
  // Used by derived classes.
  enum class AllowUnclearedAccess { kYes, kNo };

  SharedImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker);
  virtual ~SharedImageRepresentation();

  viz::ResourceFormat format() const { return backing_->format(); }
  const gfx::Size& size() const { return backing_->size(); }
  const gfx::ColorSpace& color_space() const { return backing_->color_space(); }
  GrSurfaceOrigin surface_origin() const { return backing_->surface_origin(); }
  SkAlphaType alpha_type() const { return backing_->alpha_type(); }
  uint32_t usage() const { return backing_->usage(); }
  const gpu::Mailbox& mailbox() const { return backing_->mailbox(); }
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
    ScopedAccessBase(RepresentationClass* representation)
        : representation_(representation) {
      DCHECK(!representation_->has_scoped_access_);
      representation_->has_scoped_access_ = true;
    }
    ~ScopedAccessBase() {
      DCHECK(representation_->has_scoped_access_);
      representation_->has_scoped_access_ = false;
    }

    RepresentationClass* representation() { return representation_; }
    const RepresentationClass* representation() const {
      return representation_;
    }

   private:
    RepresentationClass* const representation_;

    DISALLOW_COPY_AND_ASSIGN(ScopedAccessBase);
  };

 private:
  SharedImageManager* const manager_;
  SharedImageBacking* const backing_;
  MemoryTypeTracker* const tracker_;
  bool has_context_ = true;
  bool has_scoped_access_ = false;
};

class SharedImageRepresentationFactoryRef : public SharedImageRepresentation {
 public:
  SharedImageRepresentationFactoryRef(SharedImageManager* manager,
                                      SharedImageBacking* backing,
                                      MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  ~SharedImageRepresentationFactoryRef() override;

  const Mailbox& mailbox() const { return backing()->mailbox(); }
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) {
    backing()->Update(std::move(in_fence));
    backing()->OnWriteSucceeded();
  }
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) {
    return backing()->ProduceLegacyMailbox(mailbox_manager);
  }
  bool PresentSwapChain() { return backing()->PresentSwapChain(); }
  void RegisterImageFactory(SharedImageFactory* factory) {
    backing()->RegisterImageFactory(factory);
  }

#if defined(OS_ANDROID)
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer();
#endif
};

class GPU_GLES2_EXPORT SharedImageRepresentationGLTextureBase
    : public SharedImageRepresentation {
 public:
  class ScopedAccess
      : public ScopedAccessBase<SharedImageRepresentationGLTextureBase> {
   public:
    ScopedAccess(util::PassKey<SharedImageRepresentationGLTextureBase> pass_key,
                 SharedImageRepresentationGLTextureBase* representation)
        : ScopedAccessBase(representation) {}
    ~ScopedAccess() {
      representation()->UpdateClearedStateOnEndAccess();
      representation()->EndAccess();
    }
  };

  SharedImageRepresentationGLTextureBase(SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      GLenum mode,
      AllowUnclearedAccess allow_uncleared);

  virtual gpu::TextureBase* GetTextureBase() = 0;

 protected:
  friend class SharedImageRepresentationSkiaGL;
  friend class SharedImageRepresentationGLTextureImpl;

  // Can be overridden to handle clear state tracking when GL access begins or
  // ends.
  virtual void UpdateClearedStateOnBeginAccess() {}
  virtual void UpdateClearedStateOnEndAccess() {}

  // TODO(ericrk): Make these pure virtual and ensure real implementations
  // exist.
  virtual bool BeginAccess(GLenum mode);
  virtual void EndAccess() {}

  virtual bool SupportsMultipleConcurrentReadAccess();
};

class GPU_GLES2_EXPORT SharedImageRepresentationGLTexture
    : public SharedImageRepresentationGLTextureBase {
 public:
  SharedImageRepresentationGLTexture(SharedImageManager* manager,
                                     SharedImageBacking* backing,
                                     MemoryTypeTracker* tracker)
      : SharedImageRepresentationGLTextureBase(manager, backing, tracker) {}

  // TODO(ericrk): Move this to the ScopedAccess object. crbug.com/1003686
  virtual gles2::Texture* GetTexture() = 0;

  gpu::TextureBase* GetTextureBase() override;

 protected:
  void UpdateClearedStateOnBeginAccess() override;
  void UpdateClearedStateOnEndAccess() override;
};

class GPU_GLES2_EXPORT SharedImageRepresentationGLTexturePassthrough
    : public SharedImageRepresentationGLTextureBase {
 public:
  SharedImageRepresentationGLTexturePassthrough(SharedImageManager* manager,
                                                SharedImageBacking* backing,
                                                MemoryTypeTracker* tracker)
      : SharedImageRepresentationGLTextureBase(manager, backing, tracker) {}

  // TODO(ericrk): Move this to the ScopedAccess object. crbug.com/1003686
  virtual const scoped_refptr<gles2::TexturePassthrough>&
  GetTexturePassthrough() = 0;

  gpu::TextureBase* GetTextureBase() override;
};

class GPU_GLES2_EXPORT SharedImageRepresentationSkia
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<SharedImageRepresentationSkia> {
   public:
    ScopedWriteAccess(util::PassKey<SharedImageRepresentationSkia> pass_key,
                      SharedImageRepresentationSkia* representation,
                      sk_sp<SkSurface> surface,
                      std::unique_ptr<GrBackendSurfaceMutableState> end_state);
    ~ScopedWriteAccess();

    SkSurface* surface() const { return surface_.get(); }
    GrBackendSurfaceMutableState* end_state() const { return end_state_.get(); }

   private:
    sk_sp<SkSurface> surface_;
    std::unique_ptr<GrBackendSurfaceMutableState> end_state_;
  };

  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<SharedImageRepresentationSkia> {
   public:
    ScopedReadAccess(util::PassKey<SharedImageRepresentationSkia> pass_key,
                     SharedImageRepresentationSkia* representation,
                     sk_sp<SkPromiseImageTexture> promise_image_texture,
                     std::unique_ptr<GrBackendSurfaceMutableState> end_state);
    ~ScopedReadAccess();

    SkPromiseImageTexture* promise_image_texture() const {
      return promise_image_texture_.get();
    }
    sk_sp<SkImage> CreateSkImage(GrDirectContext* context) const;
    GrBackendSurfaceMutableState* end_state() const { return end_state_.get(); }

   private:
    sk_sp<SkPromiseImageTexture> promise_image_texture_;
    std::unique_ptr<GrBackendSurfaceMutableState> end_state_;
  };

  SharedImageRepresentationSkia(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  // Note: See BeginWriteAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared);

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared);

  // Note: See BeginReadAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  virtual bool SupportsMultipleConcurrentReadAccess();

 protected:
  // Begin the write access. The implementations should insert semaphores into
  // begin_semaphores vector which client will wait on before writing the
  // backing. The ownership of begin_semaphores is not passed to client.
  // The implementations can also optionally insert semaphores into
  // end_semaphores. If using end_semaphores, the client must submit them with
  // drawing operations which use the backing. The ownership of end_semaphores
  // are not passed to client. And client must submit the end_semaphores before
  // calling EndWriteAccess().
  // The backing can assign end_state, and the caller must reset backing's state
  // to the end_state before calling EndWriteAccess().
  virtual sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state);
  virtual sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);
  virtual void EndWriteAccess(sk_sp<SkSurface> surface) = 0;

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
  virtual sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state);
  virtual sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);
  virtual void EndReadAccess() = 0;
};

class GPU_GLES2_EXPORT SharedImageRepresentationDawn
    : public SharedImageRepresentation {
 public:
  SharedImageRepresentationDawn(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class GPU_GLES2_EXPORT ScopedAccess
      : public ScopedAccessBase<SharedImageRepresentationDawn> {
   public:
    ScopedAccess(util::PassKey<SharedImageRepresentationDawn> pass_key,
                 SharedImageRepresentationDawn* representation,
                 WGPUTexture texture);
    ~ScopedAccess();

    WGPUTexture texture() const { return texture_; }

   private:
    WGPUTexture texture_ = 0;
  };

  // Calls BeginAccess and returns a ScopedAccess object which will EndAccess
  // when it goes out of scope. The Representation must outlive the returned
  // ScopedAccess.
  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      WGPUTextureUsage usage,
      AllowUnclearedAccess allow_uncleared);

 private:
  // This can return null in case of a Dawn validation error, for example if
  // usage is invalid.
  virtual WGPUTexture BeginAccess(WGPUTextureUsage usage) = 0;
  virtual void EndAccess() = 0;
};

class GPU_GLES2_EXPORT SharedImageRepresentationOverlay
    : public SharedImageRepresentation {
 public:
  SharedImageRepresentationOverlay(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<SharedImageRepresentationOverlay> {
   public:
    ScopedReadAccess(util::PassKey<SharedImageRepresentationOverlay> pass_key,
                     SharedImageRepresentationOverlay* representation,
                     gl::GLImage* gl_image,
                     std::unique_ptr<gfx::GpuFence> fence);
    ~ScopedReadAccess();

    gl::GLImage* gl_image() const { return gl_image_; }

    std::unique_ptr<gfx::GpuFence> TakeFence() { return std::move(fence_); }

   private:
    gl::GLImage* gl_image_;
    std::unique_ptr<gfx::GpuFence> fence_;
  };

#if defined(OS_ANDROID)
  virtual void NotifyOverlayPromotion(bool promotion,
                                      const gfx::Rect& bounds) = 0;
#endif

  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess(bool needs_gl_image);

 protected:
  // TODO(weiliangc): Currently this only handles Android pre-SurfaceControl
  // case. Add appropriate fence later.
  virtual bool BeginReadAccess() = 0;
  virtual void EndReadAccess() = 0;

  // TODO(weiliangc): Add API to backing AHardwareBuffer.

  // TODO(penghuang): Refactor it to not depend on GL.
  // Get the backing as GLImage for GLSurface::ScheduleOverlayPlane.
  virtual gl::GLImage* GetGLImage() = 0;
  // Optionally returns a fence to synchronize writes on the SharedImage with
  // overlay presentation.
  virtual std::unique_ptr<gfx::GpuFence> GetReadFence() = 0;
};

// An interface that allows a SharedImageBacking to hold a reference to VA-API
// surface without depending on //media/gpu/vaapi targets.
class VaapiDependencies {
 public:
  virtual ~VaapiDependencies() = default;
  virtual const media::VASurface* GetVaSurface() const = 0;
  virtual bool SyncSurface() = 0;
};

// Interface that allows a SharedImageBacking to create VaapiDependencies from a
// NativePixmap without depending on //media/gpu/vaapi targets.
class VaapiDependenciesFactory {
 public:
  virtual ~VaapiDependenciesFactory() = default;
  // Returns a VaapiDependencies or nullptr on failure.
  virtual std::unique_ptr<VaapiDependencies> CreateVaapiDependencies(
      scoped_refptr<gfx::NativePixmap> pixmap) = 0;
};

// Representation of a SharedImageBacking as a VA-API surface.
// This representation is currently only supported by SharedImageBackingOzone.
//
// Synchronized access is currently not required in this representation because:
//
// For reads:
// We will be using this for the destination of decoding work, so no read access
// synchronization is needed from the point of view of the VA-API.
//
// For writes:
// Because of the design of the current video pipeline, we don't start the
// decoding work until we're sure that the destination buffer is not being used
// by the rest of the pipeline. However, we still need to keep track of write
// accesses so that other representations can synchronize with the decoder.
class GPU_GLES2_EXPORT SharedImageRepresentationVaapi
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<SharedImageRepresentationVaapi> {
   public:
    ScopedWriteAccess(util::PassKey<SharedImageRepresentationVaapi> pass_key,
                      SharedImageRepresentationVaapi* representation);

    ~ScopedWriteAccess();

    const media::VASurface* va_surface();
  };
  SharedImageRepresentationVaapi(SharedImageManager* manager,
                                 SharedImageBacking* backing,
                                 MemoryTypeTracker* tracker,
                                 VaapiDependencies* vaapi_dependency);
  ~SharedImageRepresentationVaapi() override;

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess();

 private:
  VaapiDependencies* vaapi_deps_;
  virtual void EndAccess() = 0;
  virtual void BeginAccess() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_
