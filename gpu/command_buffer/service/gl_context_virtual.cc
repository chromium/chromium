// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_context_virtual.h"

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_timing.h"

namespace gpu {

GLContextVirtual::GLContextVirtual(
    gl::GLShareGroup* share_group,
    gl::GLContext* shared_context,
    base::WeakPtr<GLContextVirtualDelegate> delegate)
    : GLContext(share_group),
      shared_context_(shared_context),
      delegate_(delegate) {}

bool GLContextVirtual::InitializeImpl(gl::GLSurface* compatible_surface,
                                      const gl::GLContextAttribs& attribs) {
  SetGLStateRestorer(new GLStateRestorerImpl(delegate_));
  return shared_context_->MakeVirtuallyCurrent(this, compatible_surface);
}

void GLContextVirtual::Destroy() {
  OnContextWillDestroy();
  shared_context_->OnReleaseVirtuallyCurrent(this);
  shared_context_ = nullptr;
}

bool GLContextVirtual::MakeCurrentImpl(gl::GLSurface* surface) {
  if (delegate_.get())
    return shared_context_->MakeVirtuallyCurrent(this, surface);

  LOG(ERROR) << "Trying to make virtual context current without decoder.";
  return false;
}

void GLContextVirtual::ReleaseCurrent(gl::GLSurface* surface) {
  if (IsCurrent(surface)) {
    shared_context_->OnReleaseVirtuallyCurrent(this);
    shared_context_->ReleaseCurrent(surface);
  }
}

bool GLContextVirtual::IsCurrent(gl::GLSurface* surface) {
  // If it's a real surface it needs to be current.
  if (surface &&
      !surface->IsOffscreen())
    return shared_context_->IsCurrent(surface);

  // Otherwise, only insure the context itself is current.
  return shared_context_->IsCurrent(nullptr);
}

void* GLContextVirtual::GetHandle() {
  return shared_context_->GetHandle();
}

scoped_refptr<gl::GPUTimingClient> GLContextVirtual::CreateGPUTimingClient() {
  return shared_context_->CreateGPUTimingClient();
}

std::string GLContextVirtual::GetGLVersion() {
  return shared_context_->GetGLVersion();
}

std::string GLContextVirtual::GetGLRenderer() {
  return shared_context_->GetGLRenderer();
}

const gfx::ExtensionSet& GLContextVirtual::GetExtensions() {
  return shared_context_->GetExtensions();
}

void GLContextVirtual::SetSafeToForceGpuSwitch() {
  // TODO(ccameron): This will not work if two contexts that disagree
  // about whether or not forced gpu switching may be done both share
  // the same underlying shared_context_.
  return shared_context_->SetSafeToForceGpuSwitch();
}

unsigned int GLContextVirtual::CheckStickyGraphicsResetStatusImpl() {
  unsigned int reset_status = shared_context_->CheckStickyGraphicsResetStatus();
  if (reset_status == GL_NO_ERROR)
    return GL_NO_ERROR;
  // Don't pretend we know which one of the virtual contexts was responsible.
  return GL_UNKNOWN_CONTEXT_RESET_ARB;
}

void GLContextVirtual::SetUnbindFboOnMakeCurrent() {
  shared_context_->SetUnbindFboOnMakeCurrent();
}

void GLContextVirtual::ForceReleaseVirtuallyCurrent() {
  shared_context_->OnReleaseVirtuallyCurrent(this);
}

#if BUILDFLAG(IS_APPLE)
void GLContextVirtual::AddMetalSharedEventsForBackpressure(
    std::vector<std::unique_ptr<BackpressureMetalSharedEvent>> events) {
  shared_context_->AddMetalSharedEventsForBackpressure(std::move(events));
}

uint64_t GLContextVirtual::BackpressureFenceCreate() {
  return shared_context_->BackpressureFenceCreate();
}

void GLContextVirtual::BackpressureFenceWait(uint64_t fence) {
  shared_context_->BackpressureFenceWait(fence);
}
#endif

#if BUILDFLAG(IS_MAC)
void GLContextVirtual::FlushForDriverCrashWorkaround() {
  shared_context_->FlushForDriverCrashWorkaround();
}
#endif

gl::GLDisplayEGL* GLContextVirtual::GetGLDisplayEGL() {
  return shared_context_->GetGLDisplayEGL();
}

GLContextVirtual::~GLContextVirtual() {
  Destroy();
}

void GLContextVirtual::ResetExtensions() {
  shared_context_->ResetExtensions();
}

}  // namespace gpu
