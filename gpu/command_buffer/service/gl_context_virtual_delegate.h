// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_DELEGATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_DELEGATE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class QueryManager;

namespace gles2 {
struct ContextState;
}  // namespace gles2

class GPU_GLES2_EXPORT GLContextVirtualDelegate {
 public:
  GLContextVirtualDelegate() = default;
  virtual ~GLContextVirtualDelegate() = default;

  virtual bool initialized() const = 0;
  virtual const gles2::ContextState* GetContextState() = 0;

  // Restores all of the decoder GL state.
  virtual void RestoreState(const gles2::ContextState* prev_state) = 0;

  // Restore States.
  virtual void RestoreGlobalState() const = 0;
  virtual void ClearAllAttributes() const = 0;
  virtual void RestoreActiveTexture() const = 0;
  virtual void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const = 0;
  virtual void RestoreActiveTextureUnitBinding(unsigned int target) const = 0;
  virtual void RestoreBufferBinding(unsigned int target) = 0;
  virtual void RestoreBufferBindings() const = 0;
  virtual void RestoreFramebufferBindings() const = 0;
  virtual void RestoreRenderbufferBindings() = 0;
  virtual void RestoreProgramBindings() const = 0;
  virtual void RestoreTextureUnitBindings(unsigned unit) const = 0;
  virtual void RestoreVertexAttribArray(unsigned index) = 0;
  virtual void RestoreAllExternalTextureBindingsIfNeeded() = 0;

  virtual QueryManager* GetQueryManager() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_DELEGATE_H_
