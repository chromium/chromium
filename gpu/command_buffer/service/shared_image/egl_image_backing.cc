// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/egl_image_backing.h"

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/shared_gl_fence_egl.h"

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"
#endif

namespace gpu {

class EGLImageBacking::TextureHolder : public base::RefCounted<TextureHolder> {
 public:
  explicit TextureHolder(gles2::Texture* texture) : texture_(texture) {}
  explicit TextureHolder(
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
      : texture_passthrough_(std::move(texture_passthrough)) {}

  void MarkContextLost() {
    context_lost_ = true;
    if (texture_passthrough_)
      texture_passthrough_->MarkContextLost();
  }

  gles2::Texture* texture() { return texture_; }
  const scoped_refptr<gles2::TexturePassthrough>& texture_passthrough() const {
    return texture_passthrough_;
  }

 private:
  friend class base::RefCounted<TextureHolder>;

  ~TextureHolder() {
    if (texture_) {
      texture_.ExtractAsDangling()->RemoveLightweightRef(!context_lost_);
    }
  }

  raw_ptr<gles2::Texture> texture_ = nullptr;
  const scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  bool context_lost_ = false;
};

// Implementation of GLTextureImageRepresentation which uses GL texture
// which is an EGLImage sibling.
class EGLImageBacking::GLRepresentationShared {
 public:
  using TextureHolder = EGLImageBacking::TextureHolder;
  GLRepresentationShared(EGLImageBacking* backing,
                         scoped_refptr<TextureHolder> texture_holder)
      : backing_(backing), texture_holder_(std::move(texture_holder)) {}

  GLRepresentationShared(const GLRepresentationShared&) = delete;
  GLRepresentationShared& operator=(const GLRepresentationShared&) = delete;

  ~GLRepresentationShared() {
    EndAccess();
    if (!backing_->have_context())
      texture_holder_->MarkContextLost();
    texture_holder_.reset();
  }

  bool BeginAccess(GLenum mode) {
    if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) {
      if (!backing_->BeginRead(this))
        return false;
      mode_ = RepresentationAccessMode::kRead;
    } else if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM) {
      if (!backing_->BeginWrite())
        return false;
      mode_ = RepresentationAccessMode::kWrite;
    } else {
      NOTREACHED();
    }
    return true;
  }

  void EndAccess() {
    if (mode_ == RepresentationAccessMode::kNone)
      return;

    // Pass this fence to its backing.
    if (mode_ == RepresentationAccessMode::kRead) {
      backing_->EndRead(this);
    } else if (mode_ == RepresentationAccessMode::kWrite) {
      backing_->EndWrite();
    } else {
      NOTREACHED();
    }
    mode_ = RepresentationAccessMode::kNone;
  }

  const scoped_refptr<TextureHolder>& texture_holder() const {
    return texture_holder_;
  }

 private:
  const raw_ptr<EGLImageBacking> backing_;
  scoped_refptr<TextureHolder> texture_holder_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
};

class EGLImageBacking::GLTextureEGLImageRepresentation
    : public GLTextureImageRepresentation {
 public:
  GLTextureEGLImageRepresentation(SharedImageManager* manager,
                                  EGLImageBacking* backing,
                                  MemoryTypeTracker* tracker,
                                  scoped_refptr<TextureHolder> texture_holder)
      : GLTextureImageRepresentation(manager, backing, tracker),
        shared_(backing, std::move(texture_holder)) {}

  GLTextureEGLImageRepresentation(const GLTextureEGLImageRepresentation&) =
      delete;
  GLTextureEGLImageRepresentation& operator=(
      const GLTextureEGLImageRepresentation&) = delete;

  ~GLTextureEGLImageRepresentation() override = default;

  bool BeginAccess(GLenum mode) override { return shared_.BeginAccess(mode); }

  void EndAccess() override { shared_.EndAccess(); }

  gles2::Texture* GetTexture(int plane_index) override {
    DCHECK_EQ(plane_index, 0);
    return shared_.texture_holder()->texture();
  }

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  GLRepresentationShared shared_;
};

class EGLImageBacking::GLTexturePassthroughEGLImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughEGLImageRepresentation(
      SharedImageManager* manager,
      EGLImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<TextureHolder> texture_holder)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        shared_(backing, std::move(texture_holder)) {}

  GLTexturePassthroughEGLImageRepresentation(
      const GLTexturePassthroughEGLImageRepresentation&) = delete;
  GLTexturePassthroughEGLImageRepresentation& operator=(
      const GLTexturePassthroughEGLImageRepresentation&) = delete;

  ~GLTexturePassthroughEGLImageRepresentation() override = default;

  bool BeginAccess(GLenum mode) override { return shared_.BeginAccess(mode); }

  void EndAccess() override { shared_.EndAccess(); }

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK_EQ(plane_index, 0);
    // TODO(https://crbug.com/1172769): Remove this CHECK.
    CHECK(shared_.texture_holder()->texture_passthrough());
    return shared_.texture_holder()->texture_passthrough();
  }

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  GLRepresentationShared shared_;
};

EGLImageBacking::EGLImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    size_t estimated_size,
    const GLCommonImageBackingFactory::FormatInfo& format_info,
    const GpuDriverBugWorkarounds& workarounds,
    bool use_passthrough,
    base::span<const uint8_t> pixel_data)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      estimated_size,
                                      true /*is_thread_safe*/),
      format_info_(format_info),
      use_passthrough_(use_passthrough) {
  created_on_context_ = gl::g_current_gl_context;
  // On some GPUs (NVidia) keeping reference to egl image itself is not enough,
  // we must keep reference to at least one sibling. Note that this workaround
  // is currently enabled for all android devices.
  // When we have pixel data, we want to initialize the texture with pixel data
  // first before creating eglimage from it. Hence using GenEGLImageSibling()
  // call to do that.
  if (workarounds.dont_delete_source_texture_for_egl_image)
    source_texture_holder_ = GenEGLImageSibling(pixel_data);
  else if (!pixel_data.empty())
    auto texture_holder = GenEGLImageSibling(pixel_data);
}

EGLImageBacking::~EGLImageBacking() {
  DCHECK(!source_texture_holder_);
}

SharedImageBackingType EGLImageBacking::GetType() const {
  return SharedImageBackingType::kEGLImage;
}

void EGLImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  NOTREACHED();
}

template <class T>
std::unique_ptr<T> EGLImageBacking::ProduceGLTextureInternal(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // On some GPUs (Mali, mostly Android 9, like J7) glTexSubImage fails on egl
  // image sibling. So we use the original texture if we're on the same gl
  // context. see https://crbug.com/1117370
  // If we're on the same context we're on the same thread, so
  // source_texture_holder_ is accessed only from thread we created it and
  // doesn't need lock.
  if (created_on_context_ == gl::g_current_gl_context &&
      source_texture_holder_) {
    return std::make_unique<T>(manager, this, tracker, source_texture_holder_);
  }

  auto texture_holder =
      GenEGLImageSibling(/*pixel_data=*/base::span<const uint8_t>());
  if (!texture_holder)
    return nullptr;
  return std::make_unique<T>(manager, this, tracker, std::move(texture_holder));
}

std::unique_ptr<GLTextureImageRepresentation> EGLImageBacking::ProduceGLTexture(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return ProduceGLTextureInternal<GLTextureEGLImageRepresentation>(manager,
                                                                   tracker);
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
EGLImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                             MemoryTypeTracker* tracker) {
  return ProduceGLTextureInternal<GLTexturePassthroughEGLImageRepresentation>(
      manager, tracker);
}

std::unique_ptr<SkiaGaneshImageRepresentation>
EGLImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (use_passthrough_) {
    auto gl_representation = ProduceGLTexturePassthrough(manager, tracker);
    if (!gl_representation)
      return nullptr;
    return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                             std::move(context_state), manager,
                                             this, tracker);
  } else {
    auto gl_representation = ProduceGLTexture(manager, tracker);
    if (!gl_representation)
      return nullptr;
    return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                             std::move(context_state), manager,
                                             this, tracker);
  }
}

std::unique_ptr<DawnImageRepresentation> EGLImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats) {
#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == wgpu::BackendType::OpenGLES) {
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation;
    if (use_passthrough_) {
      gl_representation = ProduceGLTexturePassthrough(manager, tracker);
    } else {
      gl_representation = ProduceGLTexture(manager, tracker);
    }
    void* egl_image = nullptr;
    {
      AutoLock auto_lock(this);
      egl_image = egl_image_.get();
    }
    return std::make_unique<DawnEGLImageRepresentation>(
        std::move(gl_representation), egl_image, manager, this, tracker,
        device.Get());
  }
#endif  // BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  return nullptr;
}

bool EGLImageBacking::BeginWrite() {
  AutoLock auto_lock(this);

  if (is_writing_ || !active_readers_.empty()) {
    DLOG(ERROR) << "BeginWrite should only be called when there are no other "
                   "readers or writers";
    return false;
  }
  is_writing_ = true;

  // When multiple threads wants to write to the same backing, writer needs to
  // wait on previous reads and writes to be finished.
  if (!read_fences_.empty()) {
    for (const auto& read_fence : read_fences_) {
      read_fence.second->ServerWait();
    }
    // Once all the read fences have been waited upon, its safe to clear all of
    // them. Note that when there is an active writer, no one can read and hence
    // can not update |read_fences_|.
    read_fences_.clear();
  }

  if (write_fence_)
    write_fence_->ServerWait();

  return true;
}

void EGLImageBacking::EndWrite() {
  AutoLock auto_lock(this);

  if (!is_writing_) {
    DLOG(ERROR) << "Attempt to end write to a SharedImageBacking without a "
                   "successful begin write";
    return;
  }

  is_writing_ = false;
  write_fence_ = gl::GLFenceEGL::Create();
}

bool EGLImageBacking::BeginRead(const GLRepresentationShared* reader) {
  AutoLock auto_lock(this);

  if (is_writing_) {
    DLOG(ERROR) << "BeginRead should only be called when there are no writers";
    return false;
  }

  if (active_readers_.contains(reader)) {
    LOG(ERROR) << "BeginRead was called twice on the same representation";
    return false;
  }
  active_readers_.insert(reader);
  if (write_fence_)
    write_fence_->ServerWait();

  return true;
}

void EGLImageBacking::EndRead(const GLRepresentationShared* reader) {
  {
    AutoLock auto_lock(this);

    if (!active_readers_.contains(reader)) {
      DLOG(ERROR) << "Attempt to end read to a SharedImageBacking without a "
                     "successful begin read";
      return;
    }
    active_readers_.erase(reader);
  }

  AutoLock auto_lock(this);
  read_fences_[gl::g_current_gl_context] =
      base::MakeRefCounted<gl::SharedGLFenceEGL>();
}

scoped_refptr<EGLImageBacking::TextureHolder>
EGLImageBacking::GenEGLImageSibling(base::span<const uint8_t> pixel_data) {
  // Create a gles2::texture.
  GLenum target = GL_TEXTURE_2D;
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);

  gl::ScopedTextureBinder texture_binder(target, service_id);

  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Note that we needed to use |bind_egl_image| flag and add some additional
  // logic to handle it in order to make the locks
  // more granular since BindToTexture() do not need to be behind the lock.
  // We don't need to bind the |egl_image_buffer_| first time when it's created.
  bool bind_egl_image = true;
  EGLImageKHR egl_image;
  {
    AutoLock auto_lock(this);

    // |pixel_data| if present should only be used to initialize texture when we
    // create |egl_image_| from it and not after it has been already
    // created.
    DCHECK(pixel_data.empty() || !egl_image_.get());
    if (!egl_image_.get()) {
      // Note that we only want to upload pixel data to a texture during init
      // time before we create |egl_image_buffer_| from it. If pixel data is
      // empty we only allocate memory for the texture object which is required
      // to create EGLImage.
      if (format_info_.supports_storage) {
        api->glTexStorage2DEXTFn(target, 1,
                                 format_info_.adjusted_storage_internal_format,
                                 size().width(), size().height());

        if (!pixel_data.empty()) {
          ScopedUnpackState scoped_unpack_state(
              /*uploading_data=*/true);
          api->glTexSubImage2DFn(target, 0, 0, 0, size().width(),
                                 size().height(), format_info_.adjusted_format,
                                 format_info_.gl_type, pixel_data.data());
        }
      } else if (format_info_.is_compressed) {
        ScopedUnpackState scoped_unpack_state(!pixel_data.empty());
        api->glCompressedTexImage2DFn(
            target, 0, format_info_.image_internal_format, size().width(),
            size().height(), 0, pixel_data.size(), pixel_data.data());
      } else {
        ScopedUnpackState scoped_unpack_state(!pixel_data.empty());

        api->glTexImage2DFn(target, 0, format_info_.image_internal_format,
                            size().width(), size().height(), 0,
                            format_info_.adjusted_format, format_info_.gl_type,
                            pixel_data.data());
      }

      // Use service id of the texture as a source to create the EGLImage.
      const EGLint egl_attrib_list[] = {EGL_GL_TEXTURE_LEVEL_KHR, 0,
                                        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
                                        EGL_NONE};
      egl_image_ = gl::MakeScopedEGLImage(
          eglGetCurrentContext(), EGL_GL_TEXTURE_2D_KHR,
          reinterpret_cast<EGLClientBuffer>(service_id), egl_attrib_list);
      if (!egl_image_.get()) {
        api->glDeleteTexturesFn(1, &service_id);
        return {};
      }
      bind_egl_image = false;
    }
    egl_image = egl_image_.get();

    if (!pixel_data.empty()) {
      // If pixel data is being uploaded to the texture, that means we are
      // sending commands to the gpu. Hence consider it as a write and add a
      // fence to synchronize it with corresponding reads. This case happens
      // when tab windows are composited by viz for tablet ui. Initial pixel
      // data gets uploaded on the gpu main thread and being read on DrDc
      // thread.
      write_fence_ = gl::GLFenceEGL::Create();
    }
  }

  // Mark the backing as cleared if pixel data has been uploaded. Note that
  // SetCleared() acquires the lock. Hence it is kept outside of previous lock
  // above.
  if (!pixel_data.empty())
    SetCleared();

  if (bind_egl_image) {
    // If we already have the |egl_image_|, just bind it to the new
    // texture to make it an EGLImage sibling.
    glEGLImageTargetTexture2DOES(target, egl_image);
    DCHECK_EQ(static_cast<EGLint>(EGL_SUCCESS), eglGetError());
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }

  if (use_passthrough_) {
    auto texture_passthrough =
        base::MakeRefCounted<gpu::gles2::TexturePassthrough>(service_id,
                                                             GL_TEXTURE_2D);
    return base::MakeRefCounted<TextureHolder>(std::move(texture_passthrough));
  }

  auto* texture =
      gles2::CreateGLES2TextureWithLightRef(service_id, GL_TEXTURE_2D);

  // If the backing is already cleared, no need to clear it again.
  gfx::Rect cleared_rect;
  if (IsCleared())
    cleared_rect = gfx::Rect(size());

  // Set the level info.
  texture->SetLevelInfo(
      GL_TEXTURE_2D, 0, format_info_.gl_format, size().width(), size().height(),
      1, 0, format_info_.gl_format, format_info_.gl_type, cleared_rect);

  texture->SetImmutable(true /*immutable*/, false /*immutable_storage*/);
  return base::MakeRefCounted<TextureHolder>(std::move(texture));
}

void EGLImageBacking::MarkForDestruction() {
  AutoLock auto_lock(this);
  DCHECK(!have_context() || created_on_context_ == gl::g_current_gl_context);

  if (source_texture_holder_ && !have_context())
    source_texture_holder_->MarkContextLost();
  source_texture_holder_.reset();
}

}  // namespace gpu
