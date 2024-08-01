// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/framebuffer_manager.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/framebuffer_completeness_cache.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace gles2 {

DecoderFramebufferState::DecoderFramebufferState()
    : clear_state_dirty(false),
      bound_read_framebuffer(nullptr),
      bound_draw_framebuffer(nullptr) {}

DecoderFramebufferState::~DecoderFramebufferState() = default;

class RenderbufferAttachment
    : public Framebuffer::Attachment {
 public:
  explicit RenderbufferAttachment(
      Renderbuffer* renderbuffer)
      : renderbuffer_(renderbuffer) {
  }

  RenderbufferAttachment(const RenderbufferAttachment&) = delete;
  RenderbufferAttachment& operator=(const RenderbufferAttachment&) = delete;

  GLsizei width() const override { return renderbuffer_->width(); }

  GLsizei height() const override { return renderbuffer_->height(); }

  GLenum internal_format() const override {
    return renderbuffer_->internal_format();
  }

  GLenum texture_type() const override {
    return TextureManager::ExtractTypeFromStorageFormat(
        renderbuffer_->internal_format());
  }

  GLsizei samples() const override { return renderbuffer_->samples(); }

  GLuint object_name() const override { return renderbuffer_->client_id(); }

  GLint level() const override {
    NOTREACHED_IN_MIGRATION();
    return -1;
  }

  bool cleared() const override { return renderbuffer_->cleared(); }

  void SetCleared(RenderbufferManager* renderbuffer_manager,
                  TextureManager* /* texture_manager */,
                  bool cleared) override {
    DCHECK(renderbuffer_manager);
    renderbuffer_manager->SetCleared(renderbuffer_.get(), cleared);
  }

  bool IsPartiallyCleared() const override { return false; }

  bool IsTextureAttachment() const override { return false; }
  bool IsRenderbufferAttachment() const override { return true; }

  bool IsTexture(TextureRef* /* texture */) const override { return false; }

  bool IsRenderbuffer(Renderbuffer* renderbuffer) const override {
    return renderbuffer_.get() == renderbuffer;
  }

  bool IsSameAttachment(const Attachment* attachment) const override {
    if (attachment->IsRenderbufferAttachment()) {
      const RenderbufferAttachment* other =
          reinterpret_cast<const RenderbufferAttachment*>(attachment);
      return IsRenderbuffer(other->renderbuffer());
    }
    return false;
  }

  bool Is3D() const override { return false; }

  bool CanRenderTo(const FeatureInfo*) const override { return true; }

  void DetachFromFramebuffer(Framebuffer* framebuffer,
                             GLenum attachment) const override {
    renderbuffer_->RemoveFramebufferAttachmentPoint(framebuffer, attachment);
  }

  bool IsLayerValid() const override { return true; }

  bool ValidForAttachmentType(GLenum attachment_type,
                              uint32_t max_color_attachments) override {
    uint32_t need = GLES2Util::GetChannelsNeededForAttachmentType(
        attachment_type, max_color_attachments);
    DCHECK_NE(0u, need);
    uint32_t have = GLES2Util::GetChannelsForFormat(internal_format());
    return (need & have) != 0;
  }

  Renderbuffer* renderbuffer() const {
    return renderbuffer_.get();
  }

  size_t GetSignatureSize(TextureManager* texture_manager) const override {
    return renderbuffer_->GetSignatureSize();
  }

  void AddToSignature(TextureManager* texture_manager,
                      std::string* signature) const override {
    DCHECK(signature);
    renderbuffer_->AddToSignature(signature);
  }

  bool FormsFeedbackLoop(TextureRef* /* texture */,
                         GLint /* level */,
                         GLint /* layer */) const override {
    return false;
  }

 protected:
  ~RenderbufferAttachment() override = default;

 private:
  scoped_refptr<Renderbuffer> renderbuffer_;
};

class TextureAttachment
    : public Framebuffer::Attachment {
 public:
  TextureAttachment(
      TextureRef* texture_ref, GLenum target, GLint level,
      GLsizei samples, GLint layer)
      : texture_ref_(texture_ref),
        target_(target),
        level_(level),
        samples_(samples),
        layer_(layer) {
  }

  TextureAttachment(const TextureAttachment&) = delete;
  TextureAttachment& operator=(const TextureAttachment&) = delete;

  GLsizei width() const override {
    GLsizei temp_width = 0;
    GLsizei temp_height = 0;
    texture_ref_->texture()->GetLevelSize(
        target_, level_, &temp_width, &temp_height, nullptr);
    return temp_width;
  }

  GLsizei height() const override {
    GLsizei temp_width = 0;
    GLsizei temp_height = 0;
    texture_ref_->texture()->GetLevelSize(
        target_, level_, &temp_width, &temp_height, nullptr);
    return temp_height;
  }

  GLenum internal_format() const override {
    GLenum temp_type = 0;
    GLenum temp_internal_format = 0;
    texture_ref_->texture()->GetLevelType(
        target_, level_, &temp_type, &temp_internal_format);
    return temp_internal_format;
  }

  GLenum texture_type() const override {
    GLenum temp_type = 0;
    GLenum temp_internal_format = 0;
    texture_ref_->texture()->GetLevelType(
        target_, level_, &temp_type, &temp_internal_format);
    return temp_type;
  }

  GLsizei samples() const override { return samples_; }

  GLint layer() const { return layer_; }

  GLenum target() const { return target_; }

  GLint level() const override { return level_; }

  GLuint object_name() const override { return texture_ref_->client_id(); }

  bool cleared() const override {
    return texture_ref_->texture()->IsLevelCleared(target_, level_);
  }

  void SetCleared(RenderbufferManager* /* renderbuffer_manager */,
                  TextureManager* texture_manager,
                  bool cleared) override {
    DCHECK(texture_manager);
    texture_manager->SetLevelCleared(
        texture_ref_.get(), target_, level_, cleared);
  }

  bool IsPartiallyCleared() const override {
    return texture_ref_->texture()->IsLevelPartiallyCleared(target_, level_);
  }

  bool IsTextureAttachment() const override { return true; }
  bool IsRenderbufferAttachment() const override { return false; }

  bool IsTexture(TextureRef* texture) const override {
    return texture == texture_ref_.get();
  }

  bool IsSameAttachment(const Attachment* attachment) const override {
    if (attachment->IsTextureAttachment()) {
      const TextureAttachment* other =
          reinterpret_cast<const TextureAttachment*>(attachment);
      return IsTexture(other->texture()) &&
          layer_ == other->layer() &&
          target_ == other->target() &&
          level_ == other->level();
    }
    return false;
  }

  bool IsRenderbuffer(Renderbuffer* /* renderbuffer */) const override {
    return false;
  }

  bool Is3D() const override {
    return (target_ == GL_TEXTURE_3D || target_ == GL_TEXTURE_2D_ARRAY);
  }

  TextureRef* texture() const {
    return texture_ref_.get();
  }

  bool CanRenderTo(const FeatureInfo* feature_info) const override {
    return texture_ref_->texture()->CanRenderTo(feature_info, level_);
  }

  void DetachFromFramebuffer(Framebuffer* framebuffer,
                             GLenum attachment) const override {
    texture_ref_->texture()->DetachFromFramebuffer();
  }

  bool IsLayerValid() const override {
    if (!Is3D()) {
      return true;
    }
    Texture* texture = texture_ref_->texture();
    DCHECK(texture);
    GLsizei width, height, depth;
    return (texture->GetLevelSize(target_, level_, &width, &height, &depth) &&
            layer_ < depth);
  }

  bool ValidForAttachmentType(GLenum attachment_type,
                              uint32_t max_color_attachments) override {
    GLenum type = 0;
    GLenum internal_format = 0;
    if (!texture_ref_->texture()->GetLevelType(
        target_, level_, &type, &internal_format)) {
      return false;
    }
    uint32_t need = GLES2Util::GetChannelsNeededForAttachmentType(
        attachment_type, max_color_attachments);
    DCHECK_NE(0u, need);
    uint32_t have = GLES2Util::GetChannelsForFormat(internal_format);

    // Workaround for NVIDIA drivers that incorrectly expose these formats as
    // renderable:
    if (internal_format == GL_LUMINANCE || internal_format == GL_ALPHA ||
        internal_format == GL_LUMINANCE_ALPHA) {
      return false;
    }
    // Disallow RGB16F, because it is only supported in ES2 and not ES3.
    if (internal_format == GL_RGB16F) {
      return false;
    }
    return (need & have) != 0;
  }

  size_t GetSignatureSize(TextureManager* texture_manager) const override {
    return texture_manager->GetSignatureSize();
  }

  void AddToSignature(TextureManager* texture_manager,
                      std::string* signature) const override {
    DCHECK(signature);
    texture_manager->AddToSignature(
        texture_ref_.get(), target_, level_, signature);
  }

  bool FormsFeedbackLoop(TextureRef* texture,
                         GLint level, GLint layer) const override {
    return texture == texture_ref_.get() &&
        level == level_ && layer == layer_;
  }

 protected:
  ~TextureAttachment() override = default;

 private:
  scoped_refptr<TextureRef> texture_ref_;
  GLenum target_;
  GLint level_;
  GLsizei samples_;
  GLint layer_;
};

FramebufferManager::FramebufferManager(
    uint32_t max_draw_buffers,
    uint32_t max_color_attachments,
    FramebufferCompletenessCache* framebuffer_combo_complete_cache)
    : framebuffer_state_change_count_(1),
      framebuffer_count_(0),
      have_context_(true),
      max_draw_buffers_(max_draw_buffers),
      max_color_attachments_(max_color_attachments),
      framebuffer_combo_complete_cache_(framebuffer_combo_complete_cache) {
  DCHECK_GT(max_draw_buffers_, 0u);
  DCHECK_GT(max_color_attachments_, 0u);
}

FramebufferManager::~FramebufferManager() {
  DCHECK(framebuffers_.empty());
  // If this triggers, that means something is keeping a reference to a
  // Framebuffer belonging to this.
  CHECK_EQ(framebuffer_count_, 0u);
}

void Framebuffer::MarkAsDeleted() {
  deleted_ = true;
  while (!attachments_.empty()) {
    auto entry = attachments_.begin();
    Attachment* attachment = entry->second.get();
    attachment->DetachFromFramebuffer(this, entry->first);
    attachments_.erase(entry);
  }
}

void FramebufferManager::Destroy(bool have_context) {
  have_context_ = have_context;
  framebuffers_.clear();
}

void FramebufferManager::StartTracking(
    Framebuffer* /* framebuffer */) {
  ++framebuffer_count_;
}

void FramebufferManager::StopTracking(
    Framebuffer* /* framebuffer */) {
  --framebuffer_count_;
}

void FramebufferManager::CreateFramebuffer(
    GLuint client_id, GLuint service_id) {
  std::pair<FramebufferMap::iterator, bool> result =
      framebuffers_.insert(
          std::make_pair(
              client_id,
              scoped_refptr<Framebuffer>(
                  new Framebuffer(this, service_id))));
  DCHECK(result.second);
}

Framebuffer::Framebuffer(FramebufferManager* manager, GLuint service_id)
    : manager_(manager),
      deleted_(false),
      service_id_(service_id),
      has_been_bound_(false),
      framebuffer_complete_state_count_id_(0),
      draw_buffer_type_mask_(0u),
      draw_buffer_float32_mask_(0u),
      draw_buffer_bound_mask_(0u),
      adjusted_draw_buffer_bound_mask_(0u),
      last_color_attachment_id_(-1),
      read_buffer_(GL_COLOR_ATTACHMENT0) {
  manager->StartTracking(this);
  DCHECK_GT(manager->max_draw_buffers_, 0u);
  draw_buffers_.reset(new GLenum[manager->max_draw_buffers_]);
  adjusted_draw_buffers_.reset(new GLenum[manager->max_draw_buffers_]);
  draw_buffers_[0] = GL_COLOR_ATTACHMENT0;
  adjusted_draw_buffers_[0] = GL_COLOR_ATTACHMENT0;
  for (uint32_t ii = 1; ii < manager->max_draw_buffers_; ++ii) {
    draw_buffers_[ii] = GL_NONE;
    adjusted_draw_buffers_[ii] = GL_NONE;
  }
}

Framebuffer::~Framebuffer() {
  if (manager_) {
    if (manager_->have_context_) {
      GLuint id = service_id();
      glDeleteFramebuffersEXT(1, &id);
    }
    manager_->StopTracking(this);
    manager_ = nullptr;
  }

  for (auto& attachment : attachments_) {
    attachment.second->DetachFromFramebuffer(this, attachment.first);
  }
}

bool Framebuffer::HasUnclearedAttachment(
    GLenum attachment_type) const {
  const Attachment* attachment = GetAttachment(attachment_type);
  return attachment && !attachment->cleared();
}

bool Framebuffer::HasDepthStencilFormatAttachment() const {
  const Attachment* depth_attachment = GetAttachment(GL_DEPTH_ATTACHMENT);
  const Attachment* stencil_attachment = GetAttachment(GL_STENCIL_ATTACHMENT);
  if (depth_attachment && stencil_attachment) {
    GLenum depth_format = depth_attachment->internal_format();
    depth_format = TextureManager::ExtractFormatFromStorageFormat(depth_format);
    GLenum stencil_format = stencil_attachment->internal_format();
    stencil_format = TextureManager::ExtractFormatFromStorageFormat(
        stencil_format);
    return depth_format == GL_DEPTH_STENCIL &&
           stencil_format == GL_DEPTH_STENCIL;
  }
  return false;
}

bool Framebuffer::HasUnclearedColorAttachments() const {
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    if (it->first >= GL_COLOR_ATTACHMENT0 &&
        it->first < GL_COLOR_ATTACHMENT0 + manager_->max_draw_buffers_) {
      const Attachment* attachment = it->second.get();
      if (!attachment->cleared())
        return true;
    }
  }
  return false;
}

bool Framebuffer::HasUnclearedIntRenderbufferAttachments() const {
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    if (!it->second->IsRenderbufferAttachment() || it->second->cleared())
      continue;
    if (GLES2Util::IsIntegerFormat(it->second->internal_format()))
      return true;
  }
  return false;
}

void Framebuffer::ClearUnclearedIntRenderbufferAttachments(
    RenderbufferManager* renderbuffer_manager) {
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    if (!it->second->IsRenderbufferAttachment() || it->second->cleared())
      continue;
    GLenum internal_format = it->second->internal_format();
    if (GLES2Util::IsIntegerFormat(internal_format)) {
      GLenum attaching_point = it->first;
      DCHECK_LE(static_cast<GLenum>(GL_COLOR_ATTACHMENT0), attaching_point);
      DCHECK_GT(GL_COLOR_ATTACHMENT0 + manager_->max_draw_buffers_,
                attaching_point);
      GLint drawbuffer = it->first - GL_COLOR_ATTACHMENT0;
      if (GLES2Util::IsUnsignedIntegerFormat(internal_format)) {
        const GLuint kZero[] = { 0u, 0u, 0u, 0u };
        glClearBufferuiv(GL_COLOR, drawbuffer, kZero);
      } else {
        DCHECK(GLES2Util::IsSignedIntegerFormat(internal_format));
        const static GLint kZero[] = { 0, 0, 0, 0 };
        glClearBufferiv(GL_COLOR, drawbuffer, kZero);
      }
      it->second->SetCleared(renderbuffer_manager, nullptr, true);
    }
  }
}

bool Framebuffer::HasSRGBAttachments() const {
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    GLenum internal_format = it->second->internal_format();
    switch (internal_format) {
      case GL_SRGB8:
      case GL_SRGB8_ALPHA8:
      case GL_SRGB_EXT:
      case GL_SRGB_ALPHA_EXT:
        return true;
      default:
        break;
    }
  }
  return false;
}

bool Framebuffer::PrepareDrawBuffersForClearingUninitializedAttachments(
    ) const {
  base::HeapArray<GLenum> buffers =
      base::HeapArray<GLenum>::Uninit(manager_->max_draw_buffers_);
  for (uint32_t i = 0; i < manager_->max_draw_buffers_; ++i)
    buffers[i] = GL_NONE;
  for (auto const& it : attachments_) {
    if (it.first >= GL_COLOR_ATTACHMENT0 &&
        it.first < GL_COLOR_ATTACHMENT0 + manager_->max_draw_buffers_ &&
        !it.second->cleared()) {
      // There should be no partially cleared images, uncleared int/3d images.
      // This is because ClearUnclearedIntOr3DImagesOrPartiallyClearedImages()
      // is called before this.
      DCHECK(!GLES2Util::IsIntegerFormat(it.second->internal_format()));
      DCHECK(!it.second->IsPartiallyCleared());
      DCHECK(!it.second->Is3D());
      buffers[it.first - GL_COLOR_ATTACHMENT0] = it.first;
    }
  }
  bool different = false;
  for (uint32_t i = 0; i < manager_->max_draw_buffers_; ++i) {
    if (buffers[i] != adjusted_draw_buffers_[i]) {
      different = true;
      break;
    }
  }
  if (different)
    glDrawBuffersARB(manager_->max_draw_buffers_, buffers.data());
  return different;
}

void Framebuffer::RestoreDrawBuffers() const {
  glDrawBuffersARB(manager_->max_draw_buffers_, adjusted_draw_buffers_.get());
}

bool Framebuffer::ValidateAndAdjustDrawBuffers(
    uint32_t fragment_output_type_mask, uint32_t fragment_output_written_mask) {
  uint32_t mask = draw_buffer_bound_mask_ & fragment_output_written_mask;
  if (mask != draw_buffer_bound_mask_ ||
      (mask & fragment_output_type_mask) != (mask & draw_buffer_type_mask_))
    return false;

  AdjustDrawBuffersImpl(mask);
  return true;
}

void Framebuffer::AdjustDrawBuffers() {
  AdjustDrawBuffersImpl(draw_buffer_bound_mask_);
}

void Framebuffer::AdjustDrawBuffersImpl(uint32_t desired_mask) {
  if (desired_mask == adjusted_draw_buffer_bound_mask_) {
    return;
  }
  // This won't be reached in every clear call - only when framebuffer has
  // changed.
  for (uint32_t ii = 0; ii < manager_->max_draw_buffers_; ++ii) {
    adjusted_draw_buffers_[ii] = draw_buffers_[ii];
    if (adjusted_draw_buffers_[ii] == GL_NONE) {
      continue;
    }
    uint32_t shift_bits = ii * 2;
    uint32_t buffer_mask = 0x3 << shift_bits;
    if ((buffer_mask & desired_mask) == 0u) {
      adjusted_draw_buffers_[ii] = GL_NONE;
    }
  }
  adjusted_draw_buffer_bound_mask_ = desired_mask;
  glDrawBuffersARB(manager_->max_draw_buffers_, adjusted_draw_buffers_.get());
}

bool Framebuffer::ContainsActiveIntegerAttachments() const {
  // 0x55555555 broadcasts SHADER_VARIABLE_FLOAT to all slots.
  uint32_t mask = 0x55555555u * SHADER_VARIABLE_FLOAT;
  mask &= draw_buffer_bound_mask_;
  return draw_buffer_type_mask_ != mask;
}

void Framebuffer::ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
    GLES2Decoder* decoder, TextureManager* texture_manager) {
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    if (!it->second->IsTextureAttachment() || it->second->cleared())
      continue;
    TextureAttachment* attachment =
        reinterpret_cast<TextureAttachment*>(it->second.get());
    if (attachment->IsPartiallyCleared() || attachment->Is3D() ||
        GLES2Util::IsIntegerFormat(attachment->internal_format())) {
      texture_manager->ClearTextureLevel(decoder,
                                         attachment->texture(),
                                         attachment->target(),
                                         attachment->level());
    }
  }
}

// TODO(jiawei.shao@intel.com): when the texture or the renderbuffer in
// format DEPTH_STENCIL, mark the specific part (depth or stencil) of it as
// cleared or uncleared instead of the whole one.
void Framebuffer::MarkAttachmentAsCleared(
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager,
    GLenum attachment,
    bool cleared) {
  AttachmentMap::iterator it = attachments_.find(attachment);
  if (it != attachments_.end()) {
    Attachment* a = it->second.get();
    if (a->cleared() != cleared) {
      a->SetCleared(renderbuffer_manager,
                    texture_manager,
                    cleared);
    }
  }
}

void Framebuffer::MarkAttachmentsAsCleared(
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager,
    bool cleared) {
  for (AttachmentMap::iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second.get();
    if (attachment->cleared() != cleared) {
      attachment->SetCleared(renderbuffer_manager, texture_manager, cleared);
    }
  }
}

bool Framebuffer::HasColorAttachment(int index) const {
  return base::Contains(attachments_, GL_COLOR_ATTACHMENT0 + index);
}

bool Framebuffer::HasDepthAttachment() const {
  return base::Contains(attachments_, GL_DEPTH_ATTACHMENT);
}

bool Framebuffer::HasStencilAttachment() const {
  return base::Contains(attachments_, GL_STENCIL_ATTACHMENT);
}

bool Framebuffer::HasActiveFloat32ColorAttachment() const {
  return draw_buffer_float32_mask_ != 0u;
}

GLenum Framebuffer::GetReadBufferInternalFormat() const {
  if (read_buffer_ == GL_NONE)
    return 0;
  AttachmentMap::const_iterator it = attachments_.find(read_buffer_);
  if (it == attachments_.end()) {
    return 0;
  }
  const Attachment* attachment = it->second.get();
  return attachment->internal_format();
}

GLenum Framebuffer::GetReadBufferTextureType() const {
  const Attachment* attachment = GetReadBufferAttachment();
  return attachment ? attachment->texture_type() : 0;
}

bool Framebuffer::GetReadBufferIsMultisampledTexture() const {
  const Attachment* attachment = GetReadBufferAttachment();
  return attachment
             ? attachment->IsTextureAttachment() && attachment->samples() > 0
             : false;
}

GLsizei Framebuffer::GetSamples() const {
  // Assume the framebuffer is complete, so return any attachment's samples.
  auto iter = attachments_.begin();
  if (iter == attachments_.end())
    return -1;
  Attachment* attachment = iter->second.get();
  DCHECK(attachment);
  return attachment->samples();
}

GLenum Framebuffer::GetDepthFormat() const {
  auto iter = attachments_.find(GL_DEPTH_ATTACHMENT);
  if (iter == attachments_.end())
    return 0;
  Attachment* attachment = iter->second.get();
  DCHECK(attachment);
  return attachment->internal_format();
}

GLenum Framebuffer::GetStencilFormat() const {
  auto iter = attachments_.find(GL_STENCIL_ATTACHMENT);
  if (iter == attachments_.end())
    return 0;
  Attachment* attachment = iter->second.get();
  DCHECK(attachment);
  return attachment->internal_format();
}

GLenum Framebuffer::IsPossiblyComplete(const FeatureInfo* feature_info) const {
  if (attachments_.empty()) {
    return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
  }

  GLsizei width = -1;
  GLsizei height = -1;
  GLsizei samples = -1;
  uint32_t colorbufferSize = 0;
  bool colorbufferSizeValid = false;

  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    GLenum attachment_type = it->first;
    Attachment* attachment = it->second.get();
    if (!attachment->ValidForAttachmentType(attachment_type,
                                            manager_->max_color_attachments_)) {
      return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    }
    if (!attachment->IsLayerValid()) {
      return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    }
    if (width < 0) {
      width = attachment->width();
      height = attachment->height();
      if (width == 0 || height == 0) {
        return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
      }
    } else if (attachment->width() != width || attachment->height() != height) {
      // Since DirectX doesn't allow attachments to be of different sizes,
      // even though ES3 allows it, it is still forbidden to ensure consistent
      // behaviors across platforms.
      // Note: Framebuffer::GetFramebufferValidSize relies on this behavior.
      return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT;
    }

    if (samples < 0) {
      samples = attachment->samples();
    } else if (attachment->samples() != samples) {
      // It's possible that the specified samples isn't the actual samples a
      // GL implementation uses, but we always return INCOMPLETE_MULTISAMPLE
      // here to ensure consistent behaviors across platforms.
      return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
    }
    if (!attachment->CanRenderTo(feature_info)) {
      return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    }

    if (it->first >= GL_COLOR_ATTACHMENT0 &&
        it->first < GL_COLOR_ATTACHMENT0 + manager_->max_color_attachments_) {
      // in GLES 2.0, all color attachments attachments must have the same
      // number of bitplanes.
      // in GLES 3.0, there is no such restriction.
      if (feature_info->context_type() == CONTEXT_TYPE_WEBGL1) {
        if (colorbufferSizeValid) {
          if (colorbufferSize !=
              GLES2Util::GetGLTypeSizeForTextures(attachment->texture_type())) {
            return GL_FRAMEBUFFER_UNSUPPORTED;
          }
        } else {
          colorbufferSize =
              GLES2Util::GetGLTypeSizeForTextures(attachment->texture_type());
          colorbufferSizeValid = true;
        }
      }

      // Attaching an image to more than one color attachment point should
      // return FRAMEBUFFER_UNSUPPORTED.
      for (GLenum i = it->first + 1;
           i < GL_COLOR_ATTACHMENT0 + manager_->max_color_attachments_; i++) {
        const Attachment* other = GetAttachment(i);
        if (other && attachment->IsSameAttachment(other)) {
          return GL_FRAMEBUFFER_UNSUPPORTED;
        }
      }
    }
  }

  // Binding different images to depth and stencil attachment points should
  // return FRAMEBUFFER_UNSUPPORTED.
  const Attachment* depth_attachment = GetAttachment(GL_DEPTH_ATTACHMENT);
  const Attachment* stencil_attachment = GetAttachment(GL_STENCIL_ATTACHMENT);
  if (depth_attachment && stencil_attachment) {
    if (!depth_attachment->IsSameAttachment(stencil_attachment)) {
      return GL_FRAMEBUFFER_UNSUPPORTED;
    }
    DCHECK_EQ(depth_attachment->internal_format(),
              stencil_attachment->internal_format());
  }
  if (feature_info->context_type() == CONTEXT_TYPE_WEBGL1) {
    // WebGL1 has specific additional restrictions on depth and stencil
    // attachments (e.g. it is forbidden to bind a DEPTH_STENCIL attachement to
    // a (pure) GL_DEPTH_ATTACHMENT. Note that in WebGL1,
    // GL_DEPTH_STENCIL_ATTACHMENT is a separate bind point, but that logic is
    // handled in Blink and translated to
    // GL_DEPTH_ATTACHMENT+GL_STENCIL_ATTACHMENT.
    uint32_t need_channels = 0;
    uint32_t have_channels = 0;
    if (depth_attachment) {
      need_channels |= GLES2Util::kDepth;
      have_channels |=
          GLES2Util::GetChannelsForFormat(depth_attachment->internal_format());
    }
    if (stencil_attachment) {
      need_channels |= GLES2Util::kStencil;
      have_channels |= GLES2Util::GetChannelsForFormat(
          stencil_attachment->internal_format());
    }
    if (need_channels != have_channels)
      return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
  }

  // This does not mean the framebuffer is actually complete. It just means our
  // checks passed.
  return GL_FRAMEBUFFER_COMPLETE;
}

GLenum Framebuffer::GetStatus(
    TextureManager* texture_manager, GLenum target) const {
  if (!manager_->GetFramebufferComboCompleteCache()) {
    return glCheckFramebufferStatusEXT(target);
  }
  // Check if we have this combo already.
  std::string signature;

  size_t signature_size = sizeof(target);
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second.get();
    signature_size +=
        sizeof(it->first) + attachment->GetSignatureSize(texture_manager);
  }

  signature.reserve(signature_size);
  signature.append(reinterpret_cast<const char*>(&target), sizeof(target));

  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second.get();
    signature.append(reinterpret_cast<const char*>(&it->first),
                     sizeof(it->first));
    attachment->AddToSignature(texture_manager, &signature);
  }
  DCHECK(signature.size() == signature_size);

  if (manager_->GetFramebufferComboCompleteCache()->IsComplete(signature)) {
    return GL_FRAMEBUFFER_COMPLETE;
  }

  GLenum result = glCheckFramebufferStatusEXT(target);

  if (result == GL_FRAMEBUFFER_COMPLETE) {
    manager_->GetFramebufferComboCompleteCache()->SetComplete(signature);
  }

  return result;
}

bool Framebuffer::IsCleared() const {
  // are all the attachments cleared?
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second.get();
    if (!attachment->cleared()) {
      return false;
    }
  }
  return true;
}

GLenum Framebuffer::GetDrawBuffer(GLenum draw_buffer) const {
  GLsizei index = static_cast<GLsizei>(
      draw_buffer - GL_DRAW_BUFFER0_ARB);
  CHECK(index >= 0 &&
        index < static_cast<GLsizei>(manager_->max_draw_buffers_));
  return draw_buffers_[index];
}

void Framebuffer::SetDrawBuffers(GLsizei n, const GLenum* bufs) {
  DCHECK(n <= static_cast<GLsizei>(manager_->max_draw_buffers_));
  for (GLsizei ii = 0; ii < n; ++ii) {
    draw_buffers_[ii] = bufs[ii];
    adjusted_draw_buffers_[ii] = bufs[ii];
  }
  for (uint32_t ii = n; ii < manager_->max_draw_buffers_; ++ii) {
    draw_buffers_[ii] = GL_NONE;
    adjusted_draw_buffers_[ii] = GL_NONE;
  }
  UpdateDrawBufferMasks();
  adjusted_draw_buffer_bound_mask_ = draw_buffer_bound_mask_;
}

bool Framebuffer::HasAlphaMRT() const {
  for (uint32_t i = 0; i < manager_->max_draw_buffers_; ++i) {
    if (draw_buffers_[i] != GL_NONE) {
      const Attachment* attachment = GetAttachment(draw_buffers_[i]);
      if (!attachment)
        continue;
      if ((GLES2Util::GetChannelsForFormat(attachment->internal_format()) &
           GLES2Util::kAlpha) != 0)
        return true;
    }
  }
  return false;
}

bool Framebuffer::HasSameInternalFormatsMRT() const {
  GLenum internal_format = 0;
  for (uint32_t i = 0; i < manager_->max_draw_buffers_; ++i) {
    if (draw_buffers_[i] != GL_NONE) {
      const Attachment* attachment = GetAttachment(draw_buffers_[i]);
      if (!attachment)
        continue;
      if (!internal_format) {
        internal_format = attachment->internal_format();
      } else if (internal_format != attachment->internal_format()) {
        return false;
      }
    }
  }
  return true;
}

void Framebuffer::UnbindRenderbuffer(
    GLenum target, Renderbuffer* renderbuffer) {
  bool done;
  do {
    done = true;
    for (AttachmentMap::const_iterator it = attachments_.begin();
         it != attachments_.end(); ++it) {
      Attachment* attachment = it->second.get();
      if (attachment->IsRenderbuffer(renderbuffer)) {
        // TODO(gman): manually detach renderbuffer.
        // glFramebufferRenderbufferEXT(target, it->first, GL_RENDERBUFFER, 0);
        AttachRenderbuffer(it->first, nullptr);
        done = false;
        break;
      }
    }
  } while (!done);
}

void Framebuffer::UnbindTexture(
    GLenum target, TextureRef* texture_ref) {
  bool done;
  do {
    done = true;
    for (AttachmentMap::const_iterator it = attachments_.begin();
         it != attachments_.end(); ++it) {
      Attachment* attachment = it->second.get();
      if (attachment->IsTexture(texture_ref)) {
        // TODO(gman): manually detach texture.
        // glFramebufferTexture2DEXT(target, it->first, GL_TEXTURE_2D, 0, 0);
        AttachTexture(it->first, nullptr, GL_TEXTURE_2D, 0, 0);
        done = false;
        break;
      }
    }
  } while (!done);
}

void Framebuffer::UpdateDrawBufferMasks() {
  draw_buffer_type_mask_ = 0u;
  draw_buffer_float32_mask_ = 0u;
  draw_buffer_bound_mask_ = 0u;
  for (uint32_t index = 0; index < manager_->max_color_attachments_; ++index) {
    GLenum draw_buffer = draw_buffers_[index];
    if (draw_buffer == GL_NONE)
      continue;
    auto iter = attachments_.find(draw_buffer);
    if (iter == attachments_.end())
      continue;
    scoped_refptr<Attachment> attachment = iter->second;
    GLenum internal_format = attachment->internal_format();
    ShaderVariableBaseType base_type = SHADER_VARIABLE_UNDEFINED_TYPE;
    if (GLES2Util::IsSignedIntegerFormat(internal_format)) {
      base_type = SHADER_VARIABLE_INT;
    } else if (GLES2Util::IsUnsignedIntegerFormat(internal_format)) {
      base_type = SHADER_VARIABLE_UINT;
    } else {
      base_type = SHADER_VARIABLE_FLOAT;
    }
    size_t shift_bits = index * 2;
    draw_buffer_type_mask_ |= base_type << shift_bits;
    draw_buffer_bound_mask_ |= 0x3 << shift_bits;
    if (GLES2Util::IsFloat32Format(internal_format)) {
      draw_buffer_float32_mask_ |= 0x3 << shift_bits;
    }
  }
}

Framebuffer* FramebufferManager::GetFramebuffer(
    GLuint client_id) {
  FramebufferMap::iterator it = framebuffers_.find(client_id);
  return it != framebuffers_.end() ? it->second.get() : nullptr;
}

void FramebufferManager::RemoveFramebuffer(GLuint client_id) {
  FramebufferMap::iterator it = framebuffers_.find(client_id);
  if (it != framebuffers_.end()) {
    it->second->MarkAsDeleted();
    framebuffers_.erase(it);
  }
}

void Framebuffer::DoUnbindGLAttachmentsForWorkaround(GLenum target) {
  // Replace all attachments with the default Renderbuffer.
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    glFramebufferRenderbufferEXT(target, it->first, GL_RENDERBUFFER, 0);
  }
}

void Framebuffer::OnInsertUpdateLastColorAttachmentId(GLenum attachment) {
  if (attachment >= GL_COLOR_ATTACHMENT0 &&
      attachment < GL_COLOR_ATTACHMENT0 + manager_->max_color_attachments_) {
    last_color_attachment_id_ =
        std::max(last_color_attachment_id_,
                 static_cast<GLsizei>(attachment - GL_COLOR_ATTACHMENT0));
  }
}

void Framebuffer::OnEraseUpdateLastColorAttachmentId(GLenum attachment) {
  if (attachment >= GL_COLOR_ATTACHMENT0 &&
      attachment < GL_COLOR_ATTACHMENT0 + manager_->max_color_attachments_ &&
      static_cast<GLsizei>(attachment - GL_COLOR_ATTACHMENT0) ==
          last_color_attachment_id_) {
    for (last_color_attachment_id_--; last_color_attachment_id_ >= 0;
         last_color_attachment_id_--) {
      if (base::Contains(attachments_,
                         GL_COLOR_ATTACHMENT0 + last_color_attachment_id_)) {
        break;
      }
    }
  }
}

void Framebuffer::AttachRenderbuffer(
    GLenum attachment, Renderbuffer* renderbuffer) {
  DCHECK_NE(static_cast<GLenum>(GL_DEPTH_STENCIL_ATTACHMENT), attachment);
  const Attachment* a = GetAttachment(attachment);
  if (a)
    a->DetachFromFramebuffer(this, attachment);
  if (renderbuffer) {
    attachments_[attachment] = scoped_refptr<Attachment>(
        new RenderbufferAttachment(renderbuffer));
    renderbuffer->AddFramebufferAttachmentPoint(this, attachment);
    OnInsertUpdateLastColorAttachmentId(attachment);
  } else {
    attachments_.erase(attachment);
    OnEraseUpdateLastColorAttachmentId(attachment);
  }
  UnmarkAsComplete();
}

void Framebuffer::AttachTexture(
    GLenum attachment, TextureRef* texture_ref, GLenum target,
    GLint level, GLsizei samples) {
  DCHECK_NE(static_cast<GLenum>(GL_DEPTH_STENCIL_ATTACHMENT), attachment);
  const Attachment* a = GetAttachment(attachment);
  if (a)
    a->DetachFromFramebuffer(this, attachment);
  if (texture_ref) {
    attachments_[attachment] = scoped_refptr<Attachment>(
        new TextureAttachment(texture_ref, target, level, samples, 0));
    texture_ref->texture()->AttachToFramebuffer();
    OnInsertUpdateLastColorAttachmentId(attachment);
  } else {
    attachments_.erase(attachment);
    OnEraseUpdateLastColorAttachmentId(attachment);
  }
  UnmarkAsComplete();
}

void Framebuffer::AttachTextureLayer(
    GLenum attachment, TextureRef* texture_ref, GLenum target,
    GLint level, GLint layer) {
  DCHECK_NE(static_cast<GLenum>(GL_DEPTH_STENCIL_ATTACHMENT), attachment);
  const Attachment* a = GetAttachment(attachment);
  if (a)
    a->DetachFromFramebuffer(this, attachment);
  if (texture_ref) {
    attachments_[attachment] = scoped_refptr<Attachment>(
        new TextureAttachment(texture_ref, target, level, 0, layer));
    texture_ref->texture()->AttachToFramebuffer();
    OnInsertUpdateLastColorAttachmentId(attachment);
  } else {
    attachments_.erase(attachment);
    OnEraseUpdateLastColorAttachmentId(attachment);
  }
  UnmarkAsComplete();
}

const Framebuffer::Attachment*
    Framebuffer::GetAttachment(
        GLenum attachment) const {
  AttachmentMap::const_iterator it = attachments_.find(attachment);
  if (it != attachments_.end()) {
    return it->second.get();
  }
  return nullptr;
}

const Framebuffer::Attachment* Framebuffer::GetReadBufferAttachment() const {
  if (read_buffer_ == GL_NONE)
    return nullptr;
  return GetAttachment(read_buffer_);
}

gfx::Size Framebuffer::GetFramebufferValidSize() const {
  // This DCHECK ensures the framebuffer was already checked to be complete.
  DCHECK(manager_->IsComplete(this));

  // IsPossiblyComplete ensures that there is at least one attachment, and that
  // all of the attachments have the same dimensions. So it's okay to just pick
  // any arbitrary attachment and return it as the min size.
  auto it = attachments_.begin();
  CHECK(it != attachments_.end(), base::NotFatalUntil::M130);
  const auto& attachment = it->second;
  return gfx::Size(attachment->width(), attachment->height());
}

bool FramebufferManager::GetClientId(
    GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (FramebufferMap::const_iterator it = framebuffers_.begin();
       it != framebuffers_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

void FramebufferManager::MarkAttachmentsAsCleared(
    Framebuffer* framebuffer,
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager) {
  DCHECK(framebuffer);
  framebuffer->MarkAttachmentsAsCleared(renderbuffer_manager,
                                        texture_manager,
                                        true);
  MarkAsComplete(framebuffer);
}

void FramebufferManager::MarkAsComplete(
    Framebuffer* framebuffer) {
  DCHECK(framebuffer);
  framebuffer->MarkAsComplete(framebuffer_state_change_count_);
}

bool FramebufferManager::IsComplete(const Framebuffer* framebuffer) {
  DCHECK(framebuffer);
  return framebuffer->framebuffer_complete_state_count_id() ==
      framebuffer_state_change_count_;
}

}  // namespace gles2
}  // namespace gpu
