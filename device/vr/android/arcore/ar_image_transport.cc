// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/ar_image_transport.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/queue.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace device {

// static
bool ArImageTransport::disable_shared_buffer_ = false;

bool ArImageTransport::UseSharedBuffer() {
  // When available (Android O and up), use AHardwareBuffer-based shared
  // images for frame transport, unless disabled due to bugs.
  static bool val = base::AndroidHardwareBufferCompat::IsSupportAvailable();
  return val && !ArImageTransport::disable_shared_buffer_;
}

ArImageTransport::ArImageTransport(
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge)
    : gl_thread_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      mailbox_bridge_(std::move(mailbox_bridge)) {
  DVLOG(2) << __func__;
}

ArImageTransport::~ArImageTransport() = default;

void ArImageTransport::DestroySharedBuffers(WebXrPresentationState* webxr) {
  DVLOG(2) << __func__;
  DCHECK(IsOnGlThread());

  if (!webxr || !UseSharedBuffer())
    return;

  std::vector<std::unique_ptr<WebXrSharedBuffer>> buffers =
      webxr->TakeSharedBuffers();
  for (auto& buffer : buffers) {
    if (!buffer->mailbox_holder.mailbox.IsZero()) {
      DCHECK(mailbox_bridge_);
      DVLOG(2) << ": DestroySharedImage, mailbox="
               << buffer->mailbox_holder.mailbox.ToDebugString();
      // Note: the sync token in mailbox_holder may not be accurate. See
      // comment in TransferFrame below.
      mailbox_bridge_->DestroySharedImage(buffer->mailbox_holder);
    }
  }
}

void ArImageTransport::Initialize(WebXrPresentationState* webxr,
                                  XrInitStatusCallback callback) {
  DCHECK(IsOnGlThread());
  DVLOG(2) << __func__;

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  ar_renderer_ = std::make_unique<ArRenderer>();
  glGenTextures(1, &camera_texture_id_arcore_);

  glGenFramebuffersEXT(1, &camera_fbo_);

  if (UseSharedBuffer()) {
    DVLOG(2) << __func__ << ": UseSharedBuffer()=true";
  } else {
    DVLOG(2) << __func__ << ": UseSharedBuffer()=false, setting up surface";
    glGenTextures(1, &transport_texture_id_);
    transport_surface_texture_ =
        gl::SurfaceTexture::Create(transport_texture_id_);
    surface_size_ = {0, 0};
    mailbox_bridge_->CreateSurface(transport_surface_texture_.get());
    transport_surface_texture_->SetFrameAvailableCallback(base::BindRepeating(
        &ArImageTransport::OnFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
  }

  mailbox_bridge_->CreateAndBindContextProvider(
      base::BindOnce(&ArImageTransport::OnMailboxBridgeReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArImageTransport::OnMailboxBridgeReady(XrInitStatusCallback callback) {
  DVLOG(2) << __func__;
  DCHECK(IsOnGlThread());

  DCHECK(mailbox_bridge_->IsConnected());

  bool success = true;
  if (UseSharedBuffer()) {
    bool shared_buffer_not_usable = mailbox_bridge_->IsGpuWorkaroundEnabled(
        gpu::DISABLE_RENDERING_TO_RGB_EXTERNAL_TEXTURE);
    DVLOG(1) << __func__
             << ": shared_buffer_not_usable=" << shared_buffer_not_usable;
    if (shared_buffer_not_usable) {
      // This is a bug workaround for shared buffer mode being unusable due to
      // device GLES driver bugs, see https://crbug.com/1355946 for an example.
      // We want to retry initialization with UseSharedBuffers forced to false.
      //
      // It would be nice if we could avoid this retry and just do the right
      // thing on the first attempt, but unfortunately that's difficult due to
      // the initialization order. We only know that the GPU bug workaround is
      // necessary after the GPU process connection is established (this is when
      // OnMailboxBridgeReady gets called). However, in order to establish the
      // GPU process connection, we already have to decide if we want to use
      // shared buffers or not - when not using shared buffers, we need to set
      // up a Surface/SurfaceTexture pair first, and doing that requires a local
      // GL context. However, setting up the local GL context wants to know if
      // we'll be using compositor mode which requires knowing if we're using
      // shared buffer mode. This is a circular dependency. Instead, just fail
      // the first initialization attempt, force UseSharedBuffers to false,
      // and try again a single time.
      ArImageTransport::disable_shared_buffer_ = true;
      DCHECK(!UseSharedBuffer());
      success = false;
    }
  }
  std::move(callback).Run(success);
}

void ArImageTransport::SetFrameAvailableCallback(
    XrFrameCallback on_transport_frame_available) {
  DVLOG(2) << __func__;
  on_transport_frame_available_ = std::move(on_transport_frame_available);
}

void ArImageTransport::OnFrameAvailable() {
  DVLOG(2) << __func__;
  DCHECK(on_transport_frame_available_);

  // This function assumes that there's only at most one frame in "processing"
  // state at any given time, the webxr_ state handling ensures that. Drawing
  // and swapping twice without an intervening UpdateTexImage call would lose
  // an image, and that would lead to images and poses getting out of sync.
  //
  // It also assumes that the ArImageTransport and Surface only exist for the
  // duration of a single session, and a new session will use fresh objects. For
  // comparison, see GvrSchedulerDelegate::OnWebXrFrameAvailable which has more
  // complex logic to support a lifetime across multiple sessions, including
  // handling a possibly-unconsumed frame left over from a previous session.

  transport_surface_texture_->UpdateTexImage();

  // The SurfaceTexture needs to be drawn using the corresponding
  // UV transform, that's usually a Y flip.
  transport_surface_texture_->GetTransformMatrix(
      &transport_surface_texture_uv_matrix_[0]);
  transport_surface_texture_uv_transform_ =
      gfx::Transform::ColMajorF(transport_surface_texture_uv_matrix_);

  on_transport_frame_available_.Run(transport_surface_texture_uv_transform_);
}

GLuint ArImageTransport::GetCameraTextureId() {
  return camera_texture_id_arcore_;
}

bool ArImageTransport::ResizeSharedBuffer(WebXrPresentationState* webxr,
                                          const gfx::Size& size,
                                          WebXrSharedBuffer* buffer) {
  DCHECK(IsOnGlThread());

  if (buffer->size == size)
    return false;

  TRACE_EVENT0("gpu", __FUNCTION__);
  // Unbind previous image (if any).
  if (!buffer->mailbox_holder.mailbox.IsZero()) {
    DVLOG(2) << ": DestroySharedImage, mailbox="
             << buffer->mailbox_holder.mailbox.ToDebugString();
    // Note: the sync token in mailbox_holder may not be accurate. See comment
    // in TransferFrame below.
    mailbox_bridge_->DestroySharedImage(buffer->mailbox_holder);
  }

  DVLOG(2) << __FUNCTION__ << ": width=" << size.width()
           << " height=" << size.height();
  // Remove reference to previous image (if any).
  buffer->local_eglimage.reset();

  static constexpr gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  static constexpr gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  gfx::GpuMemoryBufferId kBufferId(webxr->next_memory_buffer_id++);
  buffer->gmb = gpu::GpuMemoryBufferImplAndroidHardwareBuffer::Create(
      kBufferId, size, format, usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback());

  uint32_t shared_image_usage = gpu::SHARED_IMAGE_USAGE_SCANOUT |
                                gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                gpu::SHARED_IMAGE_USAGE_GLES2;
  buffer->mailbox_holder = mailbox_bridge_->CreateSharedImage(
      buffer->gmb.get(), gfx::ColorSpace(), shared_image_usage);
  DVLOG(2) << ": CreateSharedImage, mailbox="
           << buffer->mailbox_holder.mailbox.ToDebugString() << ", SyncToken="
           << buffer->mailbox_holder.sync_token.ToDebugString();

  base::android::ScopedHardwareBufferHandle ahb =
      buffer->gmb->CloneHandle().android_hardware_buffer;

  // Create an EGLImage for the buffer.
  auto egl_image = gpu::CreateEGLImageFromAHardwareBuffer(ahb.get());
  if (!egl_image.is_valid()) {
    DLOG(WARNING) << __FUNCTION__ << ": ERROR: failed to initialize image!";
    return false;
  }

  glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer->local_texture);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image.get());
  buffer->local_eglimage = std::move(egl_image);

  // Save size to avoid resize next time.
  DVLOG(1) << __FUNCTION__ << ": resized to " << size.width() << "x"
           << size.height();
  buffer->size = size;
  return true;
}

std::unique_ptr<WebXrSharedBuffer> ArImageTransport::CreateBuffer() {
  std::unique_ptr<WebXrSharedBuffer> buffer =
      std::make_unique<WebXrSharedBuffer>();
  // Local resources
  glGenTextures(1, &buffer->local_texture);
  return buffer;
}

gpu::MailboxHolder ArImageTransport::TransferFrame(
    WebXrPresentationState* webxr,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DCHECK(IsOnGlThread());
  DCHECK(UseSharedBuffer());

  if (!webxr->GetAnimatingFrame()->shared_buffer) {
    webxr->GetAnimatingFrame()->shared_buffer = CreateBuffer();
  }

  WebXrSharedBuffer* shared_buffer =
      webxr->GetAnimatingFrame()->shared_buffer.get();
  ResizeSharedBuffer(webxr, frame_size, shared_buffer);
  // Sanity check that the lazily created/resized buffer looks valid.
  DCHECK(!shared_buffer->mailbox_holder.mailbox.IsZero());
  DCHECK(shared_buffer->local_eglimage.is_valid());
  DCHECK_EQ(shared_buffer->size, frame_size);

  // We don't need to create a sync token here. ResizeSharedBuffer has created
  // one on reallocation, including initial buffer creation, and we can use
  // that. The shared image interface internally uses its own command buffer ID
  // and separate sync token release count namespace, and we must not overwrite
  // that. We don't need a new sync token when reusing a correctly-sized buffer,
  // it's only eligible for reuse after all reads from it are complete, meaning
  // that it's transitioned through "processing" and "rendering" states back
  // to "animating".
  DCHECK(shared_buffer->mailbox_holder.sync_token.HasData());
  DVLOG(2) << ": SyncToken="
           << shared_buffer->mailbox_holder.sync_token.ToDebugString();

  return shared_buffer->mailbox_holder;
}

gpu::MailboxHolder ArImageTransport::TransferCameraImageFrame(
    WebXrPresentationState* webxr,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DCHECK(IsOnGlThread());
  DCHECK(UseSharedBuffer());

  if (!webxr->GetAnimatingFrame()->camera_image_shared_buffer) {
    webxr->GetAnimatingFrame()->camera_image_shared_buffer = CreateBuffer();
  }

  WebXrSharedBuffer* camera_image_shared_buffer =
      webxr->GetAnimatingFrame()->camera_image_shared_buffer.get();
  bool was_resized =
      ResizeSharedBuffer(webxr, frame_size, camera_image_shared_buffer);
  if (was_resized) {
    // Ensure that the following GPU command buffer actions are sequenced after
    // the shared buffer operations. The shared image interface uses a separate
    // command buffer stream.
    DCHECK(camera_image_shared_buffer->mailbox_holder.sync_token.HasData());
    WaitSyncToken(camera_image_shared_buffer->mailbox_holder.sync_token);
    DVLOG(3) << __func__
             << ": "
                "camera_image_shared_buffer->mailbox_holder.sync_"
                "token="
             << camera_image_shared_buffer->mailbox_holder.sync_token
                    .ToDebugString();
  }
  // Sanity checks for the camera image buffer.
  DCHECK(!camera_image_shared_buffer->mailbox_holder.mailbox.IsZero());
  DCHECK(camera_image_shared_buffer->local_eglimage.is_valid());
  DCHECK_EQ(camera_image_shared_buffer->size, frame_size);

  // Temporarily change drawing buffer to the camera image buffer.
  if (!camera_image_fbo_) {
    glGenFramebuffersEXT(1, &camera_image_fbo_);
  }
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, camera_image_fbo_);
  glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_EXTERNAL_OES,
                            camera_image_shared_buffer->local_texture, 0);

  CopyCameraImageToFramebuffer(frame_size, uv_transform);

#if DCHECK_IS_ON()
  if (!framebuffer_complete_checked_for_camera_buffer_) {
    auto status = glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER);
    DVLOG(1) << __func__ << ": framebuffer status=" << std::hex << status;
    DCHECK(status == GL_FRAMEBUFFER_COMPLETE);
    framebuffer_complete_checked_for_camera_buffer_ = true;
  }
#endif

  // Restore default drawing buffer.
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

  std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
  std::unique_ptr<gfx::GpuFence> gpu_fence = gl_fence->GetGpuFence();
  mailbox_bridge_->WaitForClientGpuFence(*gpu_fence);

  mailbox_bridge_->GenSyncToken(
      &camera_image_shared_buffer->mailbox_holder.sync_token);
  DVLOG(3)
      << __func__ << ": camera_image_shared_buffer->mailbox_holder.sync_token="
      << camera_image_shared_buffer->mailbox_holder.sync_token.ToDebugString();
  return camera_image_shared_buffer->mailbox_holder;
}

void ArImageTransport::CreateGpuFenceForSyncToken(
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  DVLOG(2) << __func__;
  mailbox_bridge_->CreateGpuFence(sync_token, std::move(callback));
}

void ArImageTransport::WaitSyncToken(const gpu::SyncToken& sync_token) {
  mailbox_bridge_->WaitSyncToken(sync_token);
}

void ArImageTransport::CopyCameraImageToFramebuffer(
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  glDisable(GL_BLEND);
  CopyTextureToFramebuffer(camera_texture_id_arcore_, frame_size, uv_transform);
}

void ArImageTransport::ServerWaitForGpuFence(
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  std::unique_ptr<gl::GLFence> local_fence =
      gl::GLFence::CreateFromGpuFence(*gpu_fence);
  local_fence->ServerWait();
}

void ArImageTransport::CopyDrawnImageToFramebuffer(
    WebXrPresentationState* webxr,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;

  GLuint source_texture;
  if (UseSharedBuffer()) {
    WebXrSharedBuffer* shared_buffer =
        webxr->GetRenderingFrame()->shared_buffer.get();
    source_texture = shared_buffer->local_texture;
  } else {
    source_texture = transport_texture_id_;
  }

  // Set the blend mode for combining the drawn image (source) with the camera
  // image (destination). WebXR assumes that the canvas has premultiplied alpha,
  // so the source blend function is GL_ONE. The destination blend function is
  // (1 - src_alpha) as usual. (Setting that to GL_ONE would simulate an
  // additive AR headset that can't draw opaque black.)
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
  CopyTextureToFramebuffer(source_texture, frame_size, uv_transform);
}

void ArImageTransport::CopyTextureToFramebuffer(
    GLuint texture,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  // It would be a bit more efficient to do this one time on initialization,
  // but that would only be safe if ARCore and ArRenderer were guaranteed to
  // not modify these states. For now, keep the redundant operations to avoid
  // potential hard-to-find bugs.
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glViewport(0, 0, frame_size.width(), frame_size.height());

  // Draw the ARCore texture!
  float uv_transform_floats[16];
  uv_transform.GetColMajorF(uv_transform_floats);
  ar_renderer_->Draw(texture, uv_transform_floats);
}

void ArImageTransport::CopyMailboxToSurfaceAndSwap(
    const gfx::Size& frame_size,
    const gpu::MailboxHolder& mailbox,
    const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;
  if (frame_size != surface_size_) {
    DVLOG(2) << __func__ << " resize from " << surface_size_.ToString()
             << " to " << frame_size.ToString();
    transport_surface_texture_->SetDefaultBufferSize(frame_size.width(),
                                                     frame_size.height());
    mailbox_bridge_->ResizeSurface(frame_size.width(), frame_size.height());
    surface_size_ = frame_size;
  }

  // Draw the image to the surface in the GPU process's command buffer context.
  // This will trigger an OnFrameAvailable event once the corresponding
  // SurfaceTexture in the local GL context is ready for updating.
  bool swapped =
      mailbox_bridge_->CopyMailboxToSurfaceAndSwap(mailbox, uv_transform);
  DCHECK(swapped);
}

bool ArImageTransport::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArImageTransport> ArImageTransportFactory::Create(
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge) {
  return std::make_unique<ArImageTransport>(std::move(mailbox_bridge));
}

}  // namespace device
