// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gles2_external_framebuffer.h"

#include "base/not_fatal_until.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu::gles2 {
namespace {
class ScopedRestoreRenderbuffer {
 public:
  explicit ScopedRestoreRenderbuffer(gl::GLApi* api) : api_(api) {
    api_->glGetIntegervFn(GL_RENDERBUFFER_BINDING, &renderbuffer_);
  }

  ~ScopedRestoreRenderbuffer() {
    api_->glBindRenderbufferEXTFn(GL_RENDERBUFFER, renderbuffer_);
  }

 private:
  const raw_ptr<gl::GLApi> api_;
  GLint renderbuffer_ = 0;
};

class ScopedRestoreWindowRectangles {
 public:
  explicit ScopedRestoreWindowRectangles(gl::GLApi* api) : api_(api) {
    api_->glGetIntegervFn(GL_WINDOW_RECTANGLE_MODE_EXT, &mode_);

    GLint num_windows = 0;
    api_->glGetIntegervFn(GL_NUM_WINDOW_RECTANGLES_EXT, &num_windows);

    windows_.resize(4 * num_windows);
    for (int i = 0; i < num_windows; ++i) {
      glGetIntegeri_v(GL_WINDOW_RECTANGLE_EXT, i, &windows_[i * 4]);
    }
  }

  ~ScopedRestoreWindowRectangles() {
    api_->glWindowRectanglesEXTFn(mode_, windows_.size() / 4, windows_.data());
  }

 private:
  const raw_ptr<gl::GLApi> api_;
  GLint mode_ = GL_EXCLUSIVE_EXT;
  std::vector<GLint> windows_;
};

class ScopedRestoreWriteMasks {
 public:
  explicit ScopedRestoreWriteMasks(gl::GLApi* api) : api_(api) {
    api_->glGetIntegervFn(GL_STENCIL_WRITEMASK, &stencil_front_mask_);
    api_->glGetIntegervFn(GL_STENCIL_BACK_WRITEMASK, &stencil_back_mask_);
    api_->glGetBooleanvFn(GL_DEPTH_WRITEMASK, &depth_mask_);
    api_->glGetBooleanvFn(GL_COLOR_WRITEMASK, color_mask_);
  }

  ~ScopedRestoreWriteMasks() {
    api_->glColorMaskFn(color_mask_[0], color_mask_[1], color_mask_[2],
                        color_mask_[3]);
    api_->glDepthMaskFn(depth_mask_);
    api_->glStencilMaskSeparateFn(GL_FRONT, stencil_front_mask_);
    api_->glStencilMaskSeparateFn(GL_BACK, stencil_back_mask_);
  }

 private:
  const raw_ptr<gl::GLApi> api_;
  GLboolean color_mask_[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
  GLboolean depth_mask_ = GL_TRUE;
  GLint stencil_front_mask_ = 0xFF;
  GLint stencil_back_mask_ = 0xFF;
};

class ScopedRestoreClearValues {
 public:
  explicit ScopedRestoreClearValues(gl::GLApi* api) : api_(api) {
    api_->glGetFloatvFn(GL_COLOR_CLEAR_VALUE, clear_color_);
    api_->glGetFloatvFn(GL_DEPTH_CLEAR_VALUE, &clear_depth_);
    api_->glGetIntegervFn(GL_STENCIL_CLEAR_VALUE, &clear_stencil_);
  }
  ~ScopedRestoreClearValues() {
    api_->glClearColorFn(clear_color_[0], clear_color_[1], clear_color_[2],
                         clear_color_[3]);
    api_->glClearDepthFn(clear_depth_);
    api_->glClearStencilFn(clear_stencil_);
  }

 private:
  const raw_ptr<gl::GLApi> api_;
  GLfloat clear_color_[4] = {};
  GLfloat clear_depth_ = 0.0f;
  GLint clear_stencil_ = 0;
};

class ScopedRestoreFramebuffer {
 public:
  ScopedRestoreFramebuffer(gl::GLApi* api, bool supports_separate_fbo_bindings)
      : api_(api),
        supports_separate_fbo_bindings_(supports_separate_fbo_bindings) {
    if (supports_separate_fbo_bindings_) {
      api_->glGetIntegervFn(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
      api_->glGetIntegervFn(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
    } else {
      api_->glGetIntegervFn(GL_FRAMEBUFFER_BINDING, &draw_framebuffer_);
    }
  }

  ~ScopedRestoreFramebuffer() {
    if (supports_separate_fbo_bindings_) {
      api_->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER, draw_framebuffer_);
      api_->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, read_framebuffer_);
    } else {
      api_->glBindFramebufferEXTFn(GL_FRAMEBUFFER, draw_framebuffer_);
    }
  }

 private:
  const raw_ptr<gl::GLApi> api_;
  const bool supports_separate_fbo_bindings_;
  GLint draw_framebuffer_ = 0;
  GLint read_framebuffer_ = 0;
};
}  // namespace

class GLES2ExternalFramebuffer::Attachment {
 public:
  static std::unique_ptr<Attachment> CreateTexture(const gfx::Size& size,
                                                   GLenum format) {
    gl::GLApi* const api = gl::g_current_gl_context;
    gl::ScopedRestoreTexture scoped_restore(api, GL_TEXTURE_2D);

    // Don't use sized formats for textures
    GLenum texture_format;
    switch (format) {
      case GL_RGBA8:
        texture_format = GL_RGBA;
        break;
      case GL_RGB8:
        texture_format = GL_RGB;
        break;
      default:
        texture_format = GL_RGBA;
        NOTREACHED_IN_MIGRATION();
    }

    GLuint texture;
    api->glGenTexturesFn(1, &texture);
    api->glBindTextureFn(GL_TEXTURE_2D, texture);
    api->glTexImage2DFn(GL_TEXTURE_2D, 0, texture_format, size.width(),
                        size.height(), 0, texture_format, GL_UNSIGNED_BYTE,
                        nullptr);

    return std::make_unique<Attachment>(size, /*samples_count=*/0, format,
                                        /*texture=*/texture,
                                        /*renderbuffer=*/0);
  }

  static std::unique_ptr<Attachment> CreateRenderbuffer(const gfx::Size& size,
                                                        int samples_count,
                                                        GLenum format) {
    gl::GLApi* const api = gl::g_current_gl_context;
    ScopedRestoreRenderbuffer rb_restore(api);

    GLuint renderbuffer;
    api->glGenRenderbuffersEXTFn(1, &renderbuffer);
    api->glBindRenderbufferEXTFn(GL_RENDERBUFFER, renderbuffer);

    if (samples_count > 0) {
      api->glRenderbufferStorageMultisampleFn(
          GL_RENDERBUFFER, samples_count, format, size.width(), size.height());
    } else {
      api->glRenderbufferStorageEXTFn(GL_RENDERBUFFER, format, size.width(),
                                      size.height());
    }

    return std::make_unique<Attachment>(size, samples_count, format,
                                        /*texture=*/0,
                                        /*renderbuffer=*/renderbuffer);
  }

  Attachment(const gfx::Size& size,
             int samples_count,
             GLenum format,
             GLuint texture,
             GLuint renderbuffer)
      : size_(size),
        samples_count_(samples_count),
        format_(format),
        texture_(texture),
        renderbuffer_(renderbuffer) {
    DCHECK_NE(!!texture, !!renderbuffer);
    DCHECK(!size.IsEmpty());
    DCHECK(format);
  }

  Attachment(const Attachment&) = delete;
  Attachment(Attachment&&) = delete;

  Attachment& operator=(const Attachment&) = delete;
  Attachment& operator=(Attachment&&) = delete;

  ~Attachment() {
    // No need to do anything if context was lost.
    if (context_lost_)
      return;

    DCHECK_EQ(attach_point_, 0u);
    if (texture_)
      glDeleteTextures(1, &texture_);
    else if (renderbuffer_)
      glDeleteRenderbuffersEXT(1, &renderbuffer_);
  }

  void Attach(GLenum attachment) {
    DCHECK_EQ(attach_point_, 0u);
    attach_point_ = attachment;

    if (texture_)
      AttachImpl(attach_point_, /*is_texture=*/true, texture_);
    else
      AttachImpl(attach_point_, /*is_texture=*/false, renderbuffer_);
  }

  void Detach() {
    DCHECK_NE(attach_point_, 0u);

    if (texture_)
      AttachImpl(attach_point_, /*is_texture=*/true, 0);
    else
      AttachImpl(attach_point_, /*is_texture=*/false, 0);
    attach_point_ = 0;
  }

  bool NeedsResolve() { return samples_count_ > 0; }

  bool Compatible(const gfx::Size size, int samples_count, GLenum format) {
    return size_ == size && samples_count_ == samples_count &&
           format_ == format;
  }

  void OnContextLost() { context_lost_ = true; }

  GLenum format() const { return format_; }
  int samples_count() const { return samples_count_; }

 private:
  void AttachImpl(GLenum attachment, bool is_texture, GLuint object) {
    if (is_texture) {
      glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, object, 0);
    } else {
      if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                     GL_RENDERBUFFER, object);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                     GL_RENDERBUFFER, object);
      } else {
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, attachment,
                                     GL_RENDERBUFFER, object);
      }
    }
  }

  GLenum attach_point_ = 0;
  bool context_lost_ = false;

  const gfx::Size size_;
  const int samples_count_;
  const GLenum format_;
  const GLuint texture_;
  const GLuint renderbuffer_;
};

GLES2ExternalFramebuffer::GLES2ExternalFramebuffer(
    bool passthrough,
    const FeatureInfo& feature_info,
    SharedImageRepresentationFactory* shared_image_representation_factory)
    : passthrough_(passthrough),
      shared_image_representation_factory_(
          shared_image_representation_factory) {
  const bool multisampled_framebuffers_supported =
      feature_info.feature_flags().chromium_framebuffer_multisample;
  const bool rgb8_supported = feature_info.feature_flags().oes_rgb8_rgba8;
  // The only available default render buffer formats in GLES2 have very
  // little precision.  Don't enable multisampling unless 8-bit render
  // buffer formats are available--instead fall back to 8-bit textures.

  if (multisampled_framebuffers_supported && rgb8_supported) {
    glGetIntegerv(GL_MAX_SAMPLES_EXT, &max_sample_count_);
  }

  packed_depth_stencil_ = feature_info.feature_flags().packed_depth24_stencil8;
  supports_separate_fbo_bindings_ = multisampled_framebuffers_supported ||
                                    feature_info.IsWebGL2OrES3Context();
  supports_window_rectangles_ =
      feature_info.feature_flags().ext_window_rectangles;

  glGenFramebuffersEXT(1, &fbo_);
}

GLES2ExternalFramebuffer::~GLES2ExternalFramebuffer() {
  DCHECK_EQ(fbo_, 0u);
  DCHECK(attachments_.empty());
}

void GLES2ExternalFramebuffer::Destroy(bool have_context) {
  if (!have_context) {
    for (auto& attachment : attachments_)
      attachment.second->OnContextLost();

    if (shared_image_representation_)
      shared_image_representation_->OnContextLost();
  } else {
    gl::GLApi* const api = gl::g_current_gl_context;
    ScopedRestoreFramebuffer scoped_fbo_reset(api,
                                              supports_separate_fbo_bindings_);
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo_);
    for (auto& attachment : attachments_)
      attachment.second->Detach();
  }

  scoped_access_.reset();
  shared_image_representation_.reset();

  attachments_.clear();

  if (have_context)
    glDeleteFramebuffersEXT(1, &fbo_);
  fbo_ = 0;
}

bool GLES2ExternalFramebuffer::AttachSharedImage(const Mailbox& mailbox,
                                                 int samples,
                                                 bool preserve,
                                                 bool need_depth,
                                                 bool need_stencil) {
  ResolveAndDetach();

  if (mailbox.IsZero())
    return true;

  if (passthrough_) {
    shared_image_representation_ =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
  } else {
    shared_image_representation_ =
        shared_image_representation_factory_->ProduceGLTexture(mailbox);
  }

  if (!shared_image_representation_) {
    LOG(ERROR) << "Can't produce representation";
    return false;
  }

  if (!shared_image_representation_->format().is_single_plane() ||
      (shared_image_representation_->format() !=
           viz::SinglePlaneFormat::kRGBA_8888 &&
       shared_image_representation_->format() !=
           viz::SinglePlaneFormat::kRGBX_8888)) {
    LOG(ERROR) << "Unsupported format";
    return false;
  }

  scoped_access_ = shared_image_representation_->BeginScopedAccess(
      GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
      GLTextureImageRepresentationBase::AllowUnclearedAccess::kYes);

  if (!scoped_access_) {
    LOG(ERROR) << "Can't BeginAccess";
    return false;
  }

  samples = std::min(samples, max_sample_count_);
  const bool can_attach_directly = !samples && !preserve;

  GLenum clear_flags = 0;
  const auto& size = shared_image_representation_->size();

  gl::GLApi* const api = gl::g_current_gl_context;
  ScopedRestoreFramebuffer scoped_fbo_reset(api,
                                            supports_separate_fbo_bindings_);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo_);

  if (can_attach_directly) {
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        shared_image_representation_->GetTextureBase()->service_id(), 0);
    if (!shared_image_representation_->IsCleared())
      clear_flags |= GL_COLOR_BUFFER_BIT;
  } else {
    const bool has_alpha = shared_image_representation_->format() ==
                           viz::SinglePlaneFormat::kRGBA_8888;
    if (UpdateAttachment(GL_COLOR_ATTACHMENT0, size, samples,
                         has_alpha ? GL_RGBA8 : GL_RGB8)) {
      clear_flags |= GL_COLOR_BUFFER_BIT;
    }
  }

  // If GL_DEPTH24_STENCIL8 is supported, we prefer it.
  if (packed_depth_stencil_) {
    if (UpdateAttachment(
            GL_DEPTH_STENCIL_ATTACHMENT, size, samples,
            (need_depth || need_stencil) ? GL_DEPTH24_STENCIL8 : GL_NONE)) {
      clear_flags |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }
  } else {
    if (UpdateAttachment(GL_DEPTH_ATTACHMENT, size, samples,
                         need_depth ? GL_DEPTH_COMPONENT16 : GL_NONE)) {
      clear_flags |= GL_DEPTH_BUFFER_BIT;
    }
    if (UpdateAttachment(GL_STENCIL_ATTACHMENT, size, samples,
                         need_stencil ? GL_STENCIL_INDEX8 : GL_NONE)) {
      clear_flags |= GL_STENCIL_BUFFER_BIT;
    }
  }

  GLenum status = api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER);
  LOG_IF(DFATAL, status != GL_FRAMEBUFFER_COMPLETE)
      << "Framebuffer incomplete: " << status;

  if (clear_flags) {
    gl::ScopedCapability scoped_scissor(GL_SCISSOR_TEST, GL_FALSE);

    std::optional<ScopedRestoreWindowRectangles> window_rectangles_restore;
    if (supports_window_rectangles_) {
      window_rectangles_restore.emplace(api);
      api->glWindowRectanglesEXTFn(GL_EXCLUSIVE_EXT, 0, nullptr);
    }

    ScopedRestoreWriteMasks write_mask_restore(api);
    api->glColorMaskFn(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    api->glDepthMaskFn(GL_TRUE);
    api->glStencilMaskSeparateFn(GL_FRONT, 0xFF);
    api->glStencilMaskSeparateFn(GL_BACK, 0xFF);

    ScopedRestoreClearValues clear_values_restore(api);
    api->glClearColorFn(0, 0, 0, 0);
    api->glClearDepthFn(0.0f);
    api->glClearStencilFn(0);

    api->glClearFn(clear_flags);

    // If we attached SharedImage directly and did clear color attachment, mark
    // it as cleared.
    if (attachments_.find(GL_COLOR_ATTACHMENT0) == attachments_.end() &&
        (clear_flags & GL_COLOR_BUFFER_BIT))
      shared_image_representation_->SetCleared();
  }

  return true;
}

bool GLES2ExternalFramebuffer::UpdateAttachment(GLenum attachment,
                                                const gfx::Size& size,
                                                int samples,
                                                GLenum format) {
  if (auto old_attachment = attachments_.find(attachment);
      old_attachment != attachments_.end()) {
    if (old_attachment->second->Compatible(size, samples, format))
      return false;
    old_attachment->second->Detach();
    attachments_.erase(attachment);
  }

  if (format) {
    attachments_[attachment] =
        CreateAttachment(attachment, size, samples, format);
    attachments_[attachment]->Attach(attachment);
    return true;
  }
  return false;
}

std::unique_ptr<GLES2ExternalFramebuffer::Attachment>
GLES2ExternalFramebuffer::CreateAttachment(GLenum attachment,
                                           const gfx::Size& size,
                                           int samples,
                                           GLenum format) {
  if (attachment == GL_COLOR_ATTACHMENT0 && samples == 0) {
    return Attachment::CreateTexture(size, format);
  }

  return Attachment::CreateRenderbuffer(size, samples, format);
}

void GLES2ExternalFramebuffer::ResolveAndDetach() {
  if (!scoped_access_) {
    DCHECK(!shared_image_representation_);
    return;
  }

  gl::GLApi* const api = gl::g_current_gl_context;
  ScopedRestoreFramebuffer scoped_fbo_reset(api,
                                            supports_separate_fbo_bindings_);

  if (auto color_attachment = attachments_.find(GL_COLOR_ATTACHMENT0);
      color_attachment != attachments_.end()) {
    const auto& size = shared_image_representation_->size();

    // If we have separate attachment, we need to blit/resolve it to shared
    // image.
    if (color_attachment->second->NeedsResolve()) {
      DCHECK(supports_separate_fbo_bindings_);

      gl::ScopedCapability scoped_scissor(GL_SCISSOR_TEST, GL_FALSE);
      ScopedRestoreWriteMasks write_mask_restore(api);
      api->glColorMaskFn(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

      std::optional<ScopedRestoreWindowRectangles> window_rectangles_restore;
      if (supports_window_rectangles_) {
        window_rectangles_restore.emplace(api);
        api->glWindowRectanglesEXTFn(GL_EXCLUSIVE_EXT, 0, nullptr);
      }

      api->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, fbo_);

      GLuint temp_fbo;
      api->glGenFramebuffersEXTFn(1, &temp_fbo);
      api->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER, temp_fbo);
      api->glFramebufferTexture2DEXTFn(
          GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
          shared_image_representation_->GetTextureBase()->service_id(), 0);

      api->glBlitFramebufferFn(0, 0, size.width(), size.height(), 0, 0,
                               size.width(), size.height(), GL_COLOR_BUFFER_BIT,
                               GL_NEAREST);

      api->glDeleteFramebuffersEXTFn(1, &temp_fbo);
    } else {
      api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo_);
      gl::ScopedRestoreTexture texture(api, GL_TEXTURE_2D);
      api->glBindTextureFn(
          GL_TEXTURE_2D,
          shared_image_representation_->GetTextureBase()->service_id());

      api->glCopyTexSubImage2DFn(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size.width(),
                                 size.height());
    }
    // We did resolved to SharedImage, so we can mark it as cleared here.
    shared_image_representation_->SetCleared();
  } else {
    // Detach color attachment if we were attached directly.
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo_);
    api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, 0, 0);
  }
  scoped_access_.reset();
  shared_image_representation_.reset();
}

GLuint GLES2ExternalFramebuffer::GetFramebufferId() const {
  return fbo_;
}

bool GLES2ExternalFramebuffer::IsSharedImageAttached() const {
  return !!scoped_access_;
}

gfx::Size GLES2ExternalFramebuffer::GetSize() const {
  DCHECK(IsSharedImageAttached());
  return shared_image_representation_->size();
}

GLenum GLES2ExternalFramebuffer::GetColorFormat() const {
  DCHECK(IsSharedImageAttached());
  auto it = attachments_.find(GL_COLOR_ATTACHMENT0);
  CHECK(it != attachments_.end(), base::NotFatalUntil::M130);
  return it->second->format();
}

GLenum GLES2ExternalFramebuffer::GetDepthFormat() const {
  DCHECK(IsSharedImageAttached());
  if (auto it = attachments_.find(GL_DEPTH_STENCIL_ATTACHMENT);
      it != attachments_.end()) {
    return it->second->format();
  }

  if (auto it = attachments_.find(GL_DEPTH_ATTACHMENT);
      it != attachments_.end()) {
    return it->second->format();
  }

  return GL_NONE;
}

GLenum GLES2ExternalFramebuffer::GetStencilFormat() const {
  DCHECK(IsSharedImageAttached());
  DCHECK(IsSharedImageAttached());
  if (auto it = attachments_.find(GL_DEPTH_STENCIL_ATTACHMENT);
      it != attachments_.end()) {
    return it->second->format();
  }

  if (auto it = attachments_.find(GL_STENCIL_ATTACHMENT);
      it != attachments_.end()) {
    return it->second->format();
  }

  return GL_NONE;
}

int GLES2ExternalFramebuffer::GetSamplesCount() const {
  DCHECK(IsSharedImageAttached());
  auto it = attachments_.find(GL_COLOR_ATTACHMENT0);
  CHECK(it != attachments_.end(), base::NotFatalUntil::M130);
  return it->second->samples_count();
}

bool GLES2ExternalFramebuffer::HasAlpha() const {
  DCHECK(IsSharedImageAttached());
  auto it = attachments_.find(GL_COLOR_ATTACHMENT0);
  CHECK(it != attachments_.end(), base::NotFatalUntil::M130);
  return it->second->format() == GL_RGBA8;
}

bool GLES2ExternalFramebuffer::HasDepth() const {
  return GetDepthFormat() != GL_NONE;
}

bool GLES2ExternalFramebuffer::HasStencil() const {
  return GetStencilFormat() != GL_NONE;
}

}  // namespace gpu::gles2
