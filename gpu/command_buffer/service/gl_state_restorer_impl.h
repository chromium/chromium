// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLStateRestorerImpl class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_STATE_RESTORER_IMPL_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_STATE_RESTORER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_state_restorer.h"

namespace gpu {

class GLContextVirtualDelegate;

namespace gles2 {
struct ContextState;
}

// This class implements a GLStateRestorer that forwards to a DecoderContext.
class GPU_GLES2_EXPORT GLStateRestorerImpl : public gl::GLStateRestorer {
 public:
  explicit GLStateRestorerImpl(
      base::WeakPtr<GLContextVirtualDelegate> delegate);

  GLStateRestorerImpl(const GLStateRestorerImpl&) = delete;
  GLStateRestorerImpl& operator=(const GLStateRestorerImpl&) = delete;

  ~GLStateRestorerImpl() override;

  bool IsInitialized() override;
  void RestoreState(const gl::GLStateRestorer* prev_state) override;
  void RestoreAllTextureUnitAndSamplerBindings() override;
  void RestoreActiveTexture() override;
  void RestoreActiveTextureUnitBinding(unsigned int target) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  void RestoreFramebufferBindings() override;
  void RestoreProgramBindings() override;
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreVertexAttribArray(unsigned int index) override;
  void PauseQueries() override;
  void ResumeQueries() override;

 private:
  const gles2::ContextState* GetContextState() const;
  base::WeakPtr<GLContextVirtualDelegate> delegate_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_STATE_RESTORER_IMPL_H_
