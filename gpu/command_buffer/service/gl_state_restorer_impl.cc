// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_state_restorer_impl.h"

#include "gpu/command_buffer/service/gl_context_virtual_delegate.h"
#include "gpu/command_buffer/service/query_manager.h"

namespace gpu {

GLStateRestorerImpl::GLStateRestorerImpl(
    base::WeakPtr<GLContextVirtualDelegate> delegate)
    : delegate_(delegate) {}

GLStateRestorerImpl::~GLStateRestorerImpl() = default;

bool GLStateRestorerImpl::IsInitialized() {
  DCHECK(delegate_.get());
  return delegate_->initialized();
}

void GLStateRestorerImpl::RestoreState(const gl::GLStateRestorer* prev_state) {
  DCHECK(delegate_.get());
  const GLStateRestorerImpl* restorer_impl =
      static_cast<const GLStateRestorerImpl*>(prev_state);

  delegate_->RestoreState(restorer_impl ? restorer_impl->GetContextState()
                                        : nullptr);
}

void GLStateRestorerImpl::RestoreAllTextureUnitAndSamplerBindings() {
  DCHECK(delegate_.get());
  delegate_->RestoreAllTextureUnitAndSamplerBindings(nullptr);
}

void GLStateRestorerImpl::RestoreActiveTexture() {
  DCHECK(delegate_.get());
  delegate_->RestoreActiveTexture();
}

void GLStateRestorerImpl::RestoreActiveTextureUnitBinding(unsigned int target) {
  DCHECK(delegate_.get());
  delegate_->RestoreActiveTextureUnitBinding(target);
}

void GLStateRestorerImpl::RestoreAllExternalTextureBindingsIfNeeded() {
  DCHECK(delegate_.get());
  delegate_->RestoreAllExternalTextureBindingsIfNeeded();
}

void GLStateRestorerImpl::RestoreFramebufferBindings() {
  DCHECK(delegate_.get());
  delegate_->RestoreFramebufferBindings();
}

void GLStateRestorerImpl::RestoreProgramBindings() {
  DCHECK(delegate_.get());
  delegate_->RestoreProgramBindings();
}

void GLStateRestorerImpl::RestoreBufferBinding(unsigned int target) {
  DCHECK(delegate_.get());
  delegate_->RestoreBufferBinding(target);
}

void GLStateRestorerImpl::RestoreVertexAttribArray(unsigned int index) {
  DCHECK(delegate_.get());
  delegate_->RestoreVertexAttribArray(index);
}

void GLStateRestorerImpl::PauseQueries() {
  DCHECK(delegate_.get());
  if (auto* query_manager = delegate_->GetQueryManager())
    query_manager->PauseQueries();
}

void GLStateRestorerImpl::ResumeQueries() {
  DCHECK(delegate_.get());
  if (auto* query_manager = delegate_->GetQueryManager())
    query_manager->ResumeQueries();
}

const gles2::ContextState* GLStateRestorerImpl::GetContextState() const {
  DCHECK(delegate_.get());
  return delegate_->GetContextState();
}

}  // namespace gpu
