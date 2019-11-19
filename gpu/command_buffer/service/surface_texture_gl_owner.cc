// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/surface_texture_gl_owner.h"

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

SurfaceTextureGLOwner::SurfaceTextureGLOwner(
    std::unique_ptr<gles2::AbstractTexture> texture)
    : TextureOwner(true /*binds_texture_on_update */, std::move(texture)),
      surface_texture_(gl::SurfaceTexture::Create(GetTextureId())),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()) {
  DCHECK(context_);
  DCHECK(surface_);
}

SurfaceTextureGLOwner::~SurfaceTextureGLOwner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Clear the texture before we return, so that it can OnTextureDestroyed() if
  // it hasn't already.
  ClearAbstractTexture();
}

void SurfaceTextureGLOwner::OnTextureDestroyed(gles2::AbstractTexture*) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Make sure that the SurfaceTexture isn't using the GL objects.
  surface_texture_ = nullptr;
}

void SurfaceTextureGLOwner::SetFrameAvailableCallback(
    const base::RepeatingClosure& frame_available_cb) {
  DCHECK(!is_frame_available_callback_set_);

  // Setting the callback to be run from any thread since |frame_available_cb|
  // is thread safe.
  is_frame_available_callback_set_ = true;
  surface_texture_->SetFrameAvailableCallbackOnAnyThread(frame_available_cb);
}

gl::ScopedJavaSurface SurfaceTextureGLOwner::CreateJavaSurface() const {
  // |surface_texture_| might be null, but that's okay.
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void SurfaceTextureGLOwner::UpdateTexImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_texture_)
    surface_texture_->UpdateTexImage();
}

void SurfaceTextureGLOwner::EnsureTexImageBound() {
  NOTREACHED();
}

void SurfaceTextureGLOwner::GetTransformMatrix(float mtx[]) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If we don't have a SurfaceTexture, then the matrix doesn't matter.  We
  // still initialize it for good measure.
  if (surface_texture_)
    surface_texture_->GetTransformMatrix(mtx);
  else
    memset(mtx, 0, sizeof(mtx[0]) * 16);
}

void SurfaceTextureGLOwner::ReleaseBackBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_texture_)
    surface_texture_->ReleaseBackBuffers();
}

gl::GLContext* SurfaceTextureGLOwner::GetContext() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return context_.get();
}

gl::GLSurface* SurfaceTextureGLOwner::GetSurface() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return surface_.get();
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
SurfaceTextureGLOwner::GetAHardwareBuffer() {
  NOTREACHED() << "Don't use AHardwareBuffers with SurfaceTextureGLOwner";
  return nullptr;
}

}  // namespace gpu
