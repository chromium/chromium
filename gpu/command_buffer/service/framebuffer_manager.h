// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/small_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

class FeatureInfo;
class FramebufferCompletenessCache;
class FramebufferManager;
class Renderbuffer;
class RenderbufferManager;
class TextureRef;
class TextureManager;

// Info about a particular Framebuffer.
class GPU_GLES2_EXPORT Framebuffer : public base::RefCounted<Framebuffer> {
 public:
  class Attachment : public base::RefCounted<Attachment> {
   public:
    virtual GLsizei width() const = 0;
    virtual GLsizei height() const = 0;
    virtual GLenum internal_format() const = 0;
    virtual GLenum texture_type() const = 0;
    virtual GLsizei samples() const = 0;
    virtual GLuint object_name() const = 0;
    virtual GLint level() const = 0;
    virtual bool cleared() const = 0;
    virtual void SetCleared(
        RenderbufferManager* renderbuffer_manager,
        TextureManager* texture_manager,
        bool cleared) = 0;
    virtual bool IsPartiallyCleared() const = 0;
    virtual bool IsTextureAttachment() const = 0;
    virtual bool IsRenderbufferAttachment() const = 0;
    virtual bool IsTexture(TextureRef* texture) const = 0;
    virtual bool IsRenderbuffer(Renderbuffer* renderbuffer) const = 0;
    virtual bool IsSameAttachment(const Attachment* attachment) const = 0;
    virtual bool Is3D() const = 0;

    // If it's a 3D texture attachment, return true if
    // FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER is smaller than the number of
    // layers in the texture.
    virtual bool IsLayerValid() const = 0;

    virtual bool CanRenderTo(const FeatureInfo* feature_info) const = 0;
    virtual void DetachFromFramebuffer(Framebuffer* framebuffer,
                                       GLenum attachment) const = 0;
    virtual bool ValidForAttachmentType(GLenum attachment_type,
                                        uint32_t max_color_attachments) = 0;
    virtual size_t GetSignatureSize(TextureManager* texture_manager) const = 0;
    virtual void AddToSignature(
        TextureManager* texture_manager, std::string* signature) const = 0;
    virtual bool FormsFeedbackLoop(TextureRef* texture,
                                   GLint level,
                                   GLint layer) const = 0;

   protected:
    friend class base::RefCounted<Attachment>;
    virtual ~Attachment() = default;
  };

  Framebuffer(FramebufferManager* manager, GLuint service_id);

  Framebuffer(const Framebuffer&) = delete;
  Framebuffer& operator=(const Framebuffer&) = delete;

  GLuint service_id() const {
    return service_id_;
  }

  bool HasUnclearedAttachment(GLenum attachment) const;
  bool HasUnclearedColorAttachments() const;

  bool HasSRGBAttachments() const;
  bool HasDepthStencilFormatAttachment() const;

  void ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      GLES2Decoder* decoder,
      TextureManager* texture_manager);

  bool HasUnclearedIntRenderbufferAttachments() const;

  void ClearUnclearedIntRenderbufferAttachments(
    RenderbufferManager* renderbuffer_manager);

  void MarkAttachmentAsCleared(
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager,
    GLenum attachment,
    bool cleared);

  // Unbinds all attachments from this framebuffer for workaround
  // 'unbind_attachments_on_bound_render_fbo_delete'.  The Framebuffer must be
  // bound when calling this.
  void DoUnbindGLAttachmentsForWorkaround(GLenum target);

  // Attaches a renderbuffer to a particlar attachment.
  // Pass null to detach.
  void AttachRenderbuffer(
      GLenum attachment, Renderbuffer* renderbuffer);

  // Attaches a texture to a particlar attachment. Pass null to detach.
  void AttachTexture(
      GLenum attachment, TextureRef* texture_ref, GLenum target,
      GLint level, GLsizei samples);
  void AttachTextureLayer(
      GLenum attachment, TextureRef* texture_ref, GLenum target,
      GLint level, GLint layer);

  // Unbinds the given renderbuffer if it is bound.
  void UnbindRenderbuffer(
      GLenum target, Renderbuffer* renderbuffer);

  // Unbinds the given texture if it is bound.
  void UnbindTexture(
      GLenum target, TextureRef* texture_ref);

  const Attachment* GetAttachment(GLenum attachment) const;

  const Attachment* GetReadBufferAttachment() const;

  // Returns the max dimensions which fit inside all of the attachments.
  // Can only be called after the framebuffer has been checked to be complete.
  gfx::Size GetFramebufferValidSize() const;

  GLsizei GetSamples() const;

  bool IsDeleted() const {
    return deleted_;
  }

  void MarkAsValid() {
    has_been_bound_ = true;
  }

  bool IsValid() const {
    return has_been_bound_ && !IsDeleted();
  }

  bool HasColorAttachment(int index) const;
  bool HasDepthAttachment() const;
  bool HasStencilAttachment() const;
  bool HasActiveFloat32ColorAttachment() const;
  GLsizei last_color_attachment_id() const { return last_color_attachment_id_; }
  GLenum GetDepthFormat() const;
  GLenum GetStencilFormat() const;
  GLenum GetDrawBufferInternalFormat() const;
  GLenum GetReadBufferInternalFormat() const;
  // If the color attachment is a texture, returns its type; otherwise,
  // returns 0.
  GLenum GetReadBufferTextureType() const;
  bool GetReadBufferIsMultisampledTexture() const;

  // Verify all the rules in OpenGL ES 2.0.25 4.4.5 are followed.
  // Returns GL_FRAMEBUFFER_COMPLETE if there are no reasons we know we can't
  // use this combination of attachments. Otherwise returns the value
  // that glCheckFramebufferStatus should return for this set of attachments.
  // Note that receiving GL_FRAMEBUFFER_COMPLETE from this function does
  // not mean the real OpenGL will consider it framebuffer complete. It just
  // means it passed our tests.
  GLenum IsPossiblyComplete(const FeatureInfo* feature_info) const;

  // Implements optimized glGetFramebufferStatus.
  GLenum GetStatus(TextureManager* texture_manager, GLenum target) const;

  // Check all attachments are cleared
  bool IsCleared() const;

  GLenum GetDrawBuffer(GLenum draw_buffer) const;

  void SetDrawBuffers(GLsizei n, const GLenum* bufs);

  // If a color buffer is attached to GL_COLOR_ATTACHMENTi, enable that
  // draw buffer for glClear().
  // Return true if the DrawBuffers() is actually called.
  bool PrepareDrawBuffersForClearingUninitializedAttachments() const;

  // Restore |adjusted_draw_buffers_|.
  void RestoreDrawBuffers() const;

  // Checks if a draw buffer's format and its corresponding fragment shader
  // output's type are compatible, i.e., a signed integer typed variable is
  // incompatible with a float or unsigned integer buffer.
  // Return false if incompaticle.
  // Otherwise, filter out the draw buffers that are not written to but are not
  // NONE through DrawBuffers, to be on the safe side. Return true.
  // This is applied before a draw call.
  bool ValidateAndAdjustDrawBuffers(uint32_t fragment_output_type_mask,
                                    uint32_t fragment_output_written_mask);

  // Filter out the draw buffers that have no images attached but are not NONE
  // through DrawBuffers, to be on the safe side.
  // This is applied before a clear call.
  void AdjustDrawBuffers();

  bool ContainsActiveIntegerAttachments() const;

  // Return true if any draw buffers has an alpha channel.
  bool HasAlphaMRT() const;

  // Return false if any two active color attachments have different internal
  // formats.
  bool HasSameInternalFormatsMRT() const;

  void set_read_buffer(GLenum read_buffer) {
    read_buffer_ = read_buffer;
  }

  GLenum read_buffer() const {
    return read_buffer_;
  }

  // See member declaration for details.
  // The data are only valid if fbo is complete.
  uint32_t draw_buffer_type_mask() const {
    return draw_buffer_type_mask_;
  }
  uint32_t draw_buffer_bound_mask() const {
    return draw_buffer_bound_mask_;
  }

  void UnmarkAsComplete() { framebuffer_complete_state_count_id_ = 0; }

 private:
  friend class FramebufferManager;
  friend class base::RefCounted<Framebuffer>;

  ~Framebuffer();

  // Helper function updating cached last color attachment id bound.
  // Called when attachments_ changed
  void OnInsertUpdateLastColorAttachmentId(GLenum attachment);
  void OnEraseUpdateLastColorAttachmentId(GLenum attachment);

  void MarkAsDeleted();

  void MarkAttachmentsAsCleared(
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager,
    bool cleared);

  void MarkAsComplete(unsigned state_id) {
    UpdateDrawBufferMasks();
    framebuffer_complete_state_count_id_ = state_id;
  }

  unsigned framebuffer_complete_state_count_id() const {
    return framebuffer_complete_state_count_id_;
  }

  // Cache color attachments' base type mask (FLOAT, INT, UINT) and bound mask.
  // If an attachment point has no image, it's set as UNDEFINED_TYPE.
  // This call is only valid on a complete fbo.
  void UpdateDrawBufferMasks();

  // Helper for ValidateAndAdjustDrawBuffers() and AdjustDrawBuffers().
  void AdjustDrawBuffersImpl(uint32_t desired_mask);

  // The managers that owns this.
  raw_ptr<FramebufferManager> manager_;

  bool deleted_;

  // Service side framebuffer id.
  GLuint service_id_;

  // Whether this framebuffer has ever been bound.
  bool has_been_bound_;

  // state count when this framebuffer was last checked for completeness.
  unsigned framebuffer_complete_state_count_id_;

  // A map of attachments.
  using AttachmentMap =
      base::small_map<std::unordered_map<GLenum, scoped_refptr<Attachment>>, 8>;
  AttachmentMap attachments_;

  // User's draw buffers setting through DrawBuffers() call.
  std::unique_ptr<GLenum[]> draw_buffers_;

  // If a draw buffer does not have an image, or it has no corresponding
  // fragment shader output variable, it might be filtered out as NONE.
  // Note that the actually draw buffers setting sent to the driver is always
  // consistent with |adjusted_draw_buffers_|, not |draw_buffers_|.
  std::unique_ptr<GLenum[]> adjusted_draw_buffers_;

  // Draw buffer base types: FLOAT, INT, or UINT.
  // We have up to 16 draw buffers, each is encoded into 2 bits, total 32 bits:
  // the lowest 2 bits for draw buffer 0, the highest 2 bits for draw buffer 15.
  uint32_t draw_buffer_type_mask_;
  // Same layout as above, 0x03 if it's 32bit float color attachment, 0x00 if
  // not
  uint32_t draw_buffer_float32_mask_;
  // Same layout as above, 2 bits per draw buffer, 0x03 if a draw buffer has a
  // bound image, 0x00 if not.
  uint32_t draw_buffer_bound_mask_;
  // This is the mask for the actual draw buffers sent to driver.
  uint32_t adjusted_draw_buffer_bound_mask_;
  // The largest i of all GL_COLOR_ATTACHMENTi
  GLsizei last_color_attachment_id_;

  GLenum read_buffer_;
};

struct DecoderFramebufferState {
  DecoderFramebufferState();
  ~DecoderFramebufferState();

  // State saved for clearing so we can clear render buffers and then
  // restore to these values.
  bool clear_state_dirty;

  // The currently bound framebuffers
  scoped_refptr<Framebuffer> bound_read_framebuffer;
  scoped_refptr<Framebuffer> bound_draw_framebuffer;
};

// This class keeps track of the frambebuffers and their attached renderbuffers
// so we can correctly clear them.
class GPU_GLES2_EXPORT FramebufferManager {
 public:
  FramebufferManager(
      uint32_t max_draw_buffers,
      uint32_t max_color_attachments,
      FramebufferCompletenessCache* framebuffer_combo_complete_cache);

  FramebufferManager(const FramebufferManager&) = delete;
  FramebufferManager& operator=(const FramebufferManager&) = delete;

  ~FramebufferManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a Framebuffer for the given framebuffer.
  void CreateFramebuffer(GLuint client_id, GLuint service_id);

  // Gets the framebuffer info for the given framebuffer.
  Framebuffer* GetFramebuffer(GLuint client_id);

  // Removes a framebuffer info for the given framebuffer.
  void RemoveFramebuffer(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  void MarkAttachmentsAsCleared(
    Framebuffer* framebuffer,
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager);

  void MarkAsComplete(Framebuffer* framebuffer);

  bool IsComplete(const Framebuffer* framebuffer);

  void IncFramebufferStateChangeCount() {
    // make sure this is never 0.
    framebuffer_state_change_count_ =
        (framebuffer_state_change_count_ + 1) | 0x80000000U;
  }

 private:
  friend class Framebuffer;

  void StartTracking(Framebuffer* framebuffer);
  void StopTracking(Framebuffer* framebuffer);

  FramebufferCompletenessCache* GetFramebufferComboCompleteCache() {
    return framebuffer_combo_complete_cache_;
  }

  // Info for each framebuffer in the system.
  typedef std::unordered_map<GLuint, scoped_refptr<Framebuffer>> FramebufferMap;
  FramebufferMap framebuffers_;

  // Incremented anytime anything changes that might effect framebuffer
  // state.
  unsigned framebuffer_state_change_count_;

  // Counts the number of Framebuffer allocated with 'this' as its manager.
  // Allows to check no Framebuffer will outlive this.
  unsigned int framebuffer_count_;

  bool have_context_;

  uint32_t max_draw_buffers_;
  uint32_t max_color_attachments_;

  raw_ptr<FramebufferCompletenessCache> framebuffer_combo_complete_cache_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_
