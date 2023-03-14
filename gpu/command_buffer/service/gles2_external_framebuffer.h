// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_EXTERNAL_FRAMEBUFFER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_EXTERNAL_FRAMEBUFFER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
class SharedImageRepresentationFactory;
}

namespace gpu::gles2 {
class FeatureInfo;

// Encapsulates FrameBuffer Object and corresponding attachments for use as
// default framebuffer from command decoders. Only user of default emulated
// framebuffer is NaCL and hopefully will be deleted at some point.
class GPU_GLES2_EXPORT GLES2ExternalFramebuffer {
 public:
  GLES2ExternalFramebuffer(
      bool passthrough,
      const FeatureInfo& feature_info,
      SharedImageRepresentationFactory* shared_image_representation_factory);
  ~GLES2ExternalFramebuffer();

  // Attaches new SharedImage (if mailbox is not zero) to the framebuffer and
  // resolves and detaches previously attached image if any.
  bool AttachSharedImage(const Mailbox& mailbox,
                         int samples_count,
                         bool preserve,
                         bool need_depth,
                         bool need_stencil);

  // If the framebuffer is multisampled, resolves the framebuffer into current
  // SharedImage. If we preserve contents, blits preserved buffer to the shared
  // image. Detaches shared image from the framebuffer.
  void ResolveAndDetach();

  GLuint GetFramebufferId() const;
  bool IsSharedImageAttached() const;
  void Destroy(bool have_context);

  // Validating command decoder needs these for validation.
  gfx::Size GetSize() const;
  GLenum GetColorFormat() const;
  GLenum GetDepthFormat() const;
  GLenum GetStencilFormat() const;
  int GetSamplesCount() const;
  bool HasAlpha() const;
  bool HasDepth() const;
  bool HasStencil() const;

 private:
  class Attachment;

  bool UpdateAttachment(GLenum attachment,
                        const gfx::Size& size,
                        int samples,
                        GLenum format);
  std::unique_ptr<Attachment> CreateAttachment(GLenum attachment,
                                               const gfx::Size& size,
                                               int samples,
                                               GLenum format);

  // Caps
  const bool passthrough_;
  GLint max_sample_count_ = 0;
  bool packed_depth_stencil_ = false;
  bool supports_separate_fbo_bindings_ = false;
  bool supports_window_rectangles_ = false;

  // Main frame buffer
  GLuint fbo_ = 0;

  base::flat_map<GLenum, std::unique_ptr<Attachment>> attachments_;

  const raw_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  std::unique_ptr<GLTextureImageRepresentationBase>
      shared_image_representation_;
  std::unique_ptr<GLTextureImageRepresentationBase::ScopedAccess>
      scoped_access_;
};

}  // namespace gpu::gles2

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_EXTERNAL_FRAMEBUFFER_H_
