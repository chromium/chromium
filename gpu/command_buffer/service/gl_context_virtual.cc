// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_context_virtual.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_timing.h"

// TODO(crbug.com/892490): remove this once the cause of this bug is
// known.
#if defined(OS_ANDROID)
#include "base/debug/dump_without_crashing.h"
#endif

namespace gpu {

GLContextVirtual::GLContextVirtual(gl::GLShareGroup* share_group,
                                   gl::GLContext* shared_context,
                                   base::WeakPtr<DecoderContext> decoder)
    : GLContext(share_group),
      shared_context_(shared_context),
      decoder_(decoder) {}

bool GLContextVirtual::Initialize(gl::GLSurface* compatible_surface,
                                  const gl::GLContextAttribs& attribs) {
  SetGLStateRestorer(new GLStateRestorerImpl(decoder_));
  return shared_context_->MakeVirtuallyCurrent(this, compatible_surface);
}

void GLContextVirtual::Destroy() {
  shared_context_->OnReleaseVirtuallyCurrent(this);
  shared_context_ = nullptr;
}

bool GLContextVirtual::MakeCurrent(gl::GLSurface* surface) {
  if (decoder_.get())
    return shared_context_->MakeVirtuallyCurrent(this, surface);

  LOG(ERROR) << "Trying to make virtual context current without decoder.";
// TODO(crbug.com/892490): remove this once the cause of this bug is
// known.
#if defined(OS_ANDROID)
  base::debug::DumpWithoutCrashing();
#endif
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

bool GLContextVirtual::WasAllocatedUsingRobustnessExtension() {
  return shared_context_->WasAllocatedUsingRobustnessExtension();
}

void GLContextVirtual::SetUnbindFboOnMakeCurrent() {
  shared_context_->SetUnbindFboOnMakeCurrent();
}

gl::YUVToRGBConverter* GLContextVirtual::GetYUVToRGBConverter(
    const gfx::ColorSpace& color_space) {
  return shared_context_->GetYUVToRGBConverter(color_space);
}

void GLContextVirtual::ForceReleaseVirtuallyCurrent() {
  shared_context_->OnReleaseVirtuallyCurrent(this);
}

#if defined(OS_MACOSX)
uint64_t GLContextVirtual::BackpressureFenceCreate() {
  return shared_context_->BackpressureFenceCreate();
}

void GLContextVirtual::BackpressureFenceWait(uint64_t fence) {
  shared_context_->BackpressureFenceWait(fence);
}

void GLContextVirtual::FlushForDriverCrashWorkaround() {
  shared_context_->FlushForDriverCrashWorkaround();
}
#endif

GLContextVirtual::~GLContextVirtual() {
  Destroy();
}

void GLContextVirtual::ResetExtensions() {
  shared_context_->ResetExtensions();
}

}  // namespace gpu
