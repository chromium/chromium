// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_egl_image.h"

#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_batch_access_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/texture_definition.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/shared_gl_fence_egl.h"

namespace gpu {

class SharedImageBackingEglImage::TextureHolder
    : public base::RefCounted<TextureHolder> {
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
    if (texture_)
      texture_->RemoveLightweightRef(!context_lost_);
  }

  gles2::Texture* const texture_ = nullptr;
  const scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  bool context_lost_ = false;
};

// Implementation of SharedImageRepresentationGLTexture which uses GL texture
// which is an EGLImage sibling.
class SharedImageBackingEglImage::RepresentationGLShared {
 public:
  using TextureHolder = SharedImageBackingEglImage::TextureHolder;
  RepresentationGLShared(SharedImageBackingEglImage* backing,
                         scoped_refptr<TextureHolder> texture_holder)
      : backing_(backing), texture_holder_(std::move(texture_holder)) {}

  ~RepresentationGLShared() {
    EndAccess();
    if (!backing_->have_context())
      texture_holder_->MarkContextLost();
    texture_holder_.reset();
  }

  bool BeginAccess(GLenum mode) {
    if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
        mode == GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM) {
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
  SharedImageBackingEglImage* const backing_;
  scoped_refptr<TextureHolder> texture_holder_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
  DISALLOW_COPY_AND_ASSIGN(RepresentationGLShared);
};

class SharedImageBackingEglImage::RepresentationGLTexture
    : public SharedImageRepresentationGLTexture {
 public:
  RepresentationGLTexture(SharedImageManager* manager,
                          SharedImageBackingEglImage* backing,
                          MemoryTypeTracker* tracker,
                          scoped_refptr<TextureHolder> texture_holder)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        shared_(backing, std::move(texture_holder)) {}

  ~RepresentationGLTexture() override = default;

  bool BeginAccess(GLenum mode) override { return shared_.BeginAccess(mode); }

  void EndAccess() override { shared_.EndAccess(); }

  gles2::Texture* GetTexture() override {
    return shared_.texture_holder()->texture();
  }

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  RepresentationGLShared shared_;
  DISALLOW_COPY_AND_ASSIGN(RepresentationGLTexture);
};

class SharedImageBackingEglImage::RepresentationGLTexturePassthrough
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  RepresentationGLTexturePassthrough(
      SharedImageManager* manager,
      SharedImageBackingEglImage* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<TextureHolder> texture_holder)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        shared_(backing, std::move(texture_holder)) {}

  ~RepresentationGLTexturePassthrough() override = default;

  bool BeginAccess(GLenum mode) override { return shared_.BeginAccess(mode); }

  void EndAccess() override { shared_.EndAccess(); }

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    // TODO(https://crbug.com/1172769): Remove this CHECK.
    CHECK(shared_.texture_holder()->texture_passthrough());
    return shared_.texture_holder()->texture_passthrough();
  }

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  RepresentationGLShared shared_;
  DISALLOW_COPY_AND_ASSIGN(RepresentationGLTexturePassthrough);
};

SharedImageBackingEglImage::SharedImageBackingEglImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    size_t estimated_size,
    GLuint gl_format,
    GLuint gl_type,
    SharedImageBatchAccessManager* batch_access_manager,
    const GpuDriverBugWorkarounds& workarounds,
    bool use_passthrough)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      estimated_size,
                                      true /*is_thread_safe*/),
      gl_format_(gl_format),
      gl_type_(gl_type),
      batch_access_manager_(batch_access_manager),
      use_passthrough_(use_passthrough) {
  DCHECK(batch_access_manager_);
  created_on_context_ = gl::g_current_gl_context;
  // On some GPUs (NVidia) keeping reference to egl image itself is not enough,
  // we must keep reference to at least one sibling.
  if (workarounds.dont_delete_source_texture_for_egl_image) {
    source_texture_holder_ = GenEGLImageSibling();
  }
}

SharedImageBackingEglImage::~SharedImageBackingEglImage() {
  // Un-Register this backing from the |batch_access_manager_|.
  batch_access_manager_->UnregisterEglBacking(this);
  DCHECK(!source_texture_holder_);
}

void SharedImageBackingEglImage::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  NOTREACHED();
}

bool SharedImageBackingEglImage::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  // This backing doe not support legacy mailbox system.
  return false;
}

template <class T>
std::unique_ptr<T> SharedImageBackingEglImage::ProduceGLTextureInternal(
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

  auto texture_holder = GenEGLImageSibling();
  if (!texture_holder)
    return nullptr;
  return std::make_unique<T>(manager, this, tracker, std::move(texture_holder));
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingEglImage::ProduceGLTexture(SharedImageManager* manager,
                                             MemoryTypeTracker* tracker) {
  return ProduceGLTextureInternal<RepresentationGLTexture>(manager, tracker);
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingEglImage::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return ProduceGLTextureInternal<RepresentationGLTexturePassthrough>(manager,
                                                                      tracker);
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingEglImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (use_passthrough_) {
    auto gl_representation = ProduceGLTexturePassthrough(manager, tracker);
    if (!gl_representation)
      return nullptr;
    return SharedImageRepresentationSkiaGL::Create(std::move(gl_representation),
                                                   std::move(context_state),
                                                   manager, this, tracker);
  } else {
    auto gl_representation = ProduceGLTexture(manager, tracker);
    if (!gl_representation)
      return nullptr;
    return SharedImageRepresentationSkiaGL::Create(std::move(gl_representation),
                                                   std::move(context_state),
                                                   manager, this, tracker);
  }
}

bool SharedImageBackingEglImage::BeginWrite() {
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

void SharedImageBackingEglImage::EndWrite() {
  AutoLock auto_lock(this);

  if (!is_writing_) {
    DLOG(ERROR) << "Attempt to end write to a SharedImageBacking without a "
                   "successful begin write";
    return;
  }

  is_writing_ = false;
  write_fence_ = gl::GLFenceEGL::Create();
}

bool SharedImageBackingEglImage::BeginRead(
    const RepresentationGLShared* reader) {
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

void SharedImageBackingEglImage::EndRead(const RepresentationGLShared* reader) {
  {
    AutoLock auto_lock(this);

    if (!active_readers_.contains(reader)) {
      DLOG(ERROR) << "Attempt to end read to a SharedImageBacking without a "
                     "successful begin read";
      return;
    }
    active_readers_.erase(reader);
  }

  // For batch reads, we only need to create 1 fence after the last
  // EndRead() for the whole batch of reads. Hence we just register this backing
  // here with the |batch_access_manager_| so that it can set an end read fence
  // on this backing later after the last read of the batch. This improves
  // performance because creating and inserting gl fences are costly. For non
  // batch reads/regular reads, we create 1 fence per EndRead().
  if (batch_access_manager_->IsDoingBatchReads()) {
    batch_access_manager_->RegisterEglBackingForEndReadFence(this);
    return;
  }
  AutoLock auto_lock(this);
  read_fences_[gl::g_current_gl_context] =
      base::MakeRefCounted<gl::SharedGLFenceEGL>();
}

scoped_refptr<SharedImageBackingEglImage::TextureHolder>
SharedImageBackingEglImage::GenEGLImageSibling() {
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
  scoped_refptr<gles2::NativeImageBuffer> buffer;
  {
    AutoLock auto_lock(this);
    if (!egl_image_buffer_) {
      // Allocate memory for texture object if this is the first EGLImage
      // target/sibling. Memory for EGLImage will not be created if we don't
      // allocate memory for the texture object.
      api->glTexImage2DFn(target, 0, gl_format_, size().width(),
                          size().height(), 0, gl_format_, gl_type_, nullptr);

      // Use service id of the texture as a source to create the native buffer.
      egl_image_buffer_ = gles2::NativeImageBuffer::Create(service_id);
      if (!egl_image_buffer_) {
        api->glDeleteTexturesFn(1, &service_id);
        return {};
      }
      bind_egl_image = false;
    }
    buffer = egl_image_buffer_;
  }
  if (bind_egl_image) {
    // If we already have the |egl_image_buffer_|, just bind it to the new
    // texture to make it an EGLImage sibling.
    buffer->BindToTexture(target);
  }

  if (use_passthrough_) {
    auto texture_passthrough =
        base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
            service_id, GL_TEXTURE_2D, gl_format_, size().width(),
            size().height(),
            /*depth=*/1, /*border=*/0, gl_format_, gl_type_);
    return base::MakeRefCounted<TextureHolder>(std::move(texture_passthrough));
  }

  auto* texture = new gles2::Texture(service_id);
  texture->SetLightweightRef();
  texture->SetTarget(GL_TEXTURE_2D, 1 /*max_levels*/);
  texture->set_min_filter(GL_LINEAR);
  texture->set_mag_filter(GL_LINEAR);
  texture->set_wrap_t(GL_CLAMP_TO_EDGE);
  texture->set_wrap_s(GL_CLAMP_TO_EDGE);

  // If the backing is already cleared, no need to clear it again.
  gfx::Rect cleared_rect;
  if (IsCleared())
    cleared_rect = gfx::Rect(size());

  // Set the level info.
  texture->SetLevelInfo(GL_TEXTURE_2D, 0, gl_format_, size().width(),
                        size().height(), 1, 0, gl_format_, gl_type_,
                        cleared_rect);

  texture->SetImmutable(true /*immutable*/, false /*immutable_storage*/);
  return base::MakeRefCounted<TextureHolder>(std::move(texture));
}

void SharedImageBackingEglImage::SetEndReadFence(
    scoped_refptr<gl::SharedGLFenceEGL> shared_egl_fence) {
  AutoLock auto_lock(this);
  read_fences_[gl::g_current_gl_context] = std::move(shared_egl_fence);
}

void SharedImageBackingEglImage::MarkForDestruction() {
  AutoLock auto_lock(this);
  DCHECK(!have_context() || created_on_context_ == gl::g_current_gl_context);

  if (source_texture_holder_ && !have_context())
    source_texture_holder_->MarkContextLost();
  source_texture_holder_.reset();
}

}  // namespace gpu
