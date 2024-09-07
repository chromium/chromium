// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/xr_image_transport_base.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/public/cpp/features.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/ipc/common/android/android_hardware_buffer_utils.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"

namespace device {

// static
bool XrImageTransportBase::disable_shared_buffer_ = false;

bool XrImageTransportBase::UseSharedBuffer() {
  // When available (Android O and up), use AHardwareBuffer-based shared
  // images for frame transport, unless disabled due to bugs or by the user.
  static bool support_shared_buffer =
      base::FeatureList::IsEnabled(features::kWebXrSharedBuffers) &&
      base::AndroidHardwareBufferCompat::IsSupportAvailable();
  return support_shared_buffer && !XrImageTransportBase::disable_shared_buffer_;
}

GLenum XrImageTransportBase::SharedBufferTextureTarget() {
  return base::FeatureList::IsEnabled(
             features::kUseTargetTexture2DForSharedBuffers)
             ? GL_TEXTURE_2D
             : GL_TEXTURE_EXTERNAL_OES;
}

XrImageTransportBase::XrImageTransportBase(
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge)
    : mailbox_bridge_(std::move(mailbox_bridge)),
      gl_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DVLOG(2) << __func__;
}

XrImageTransportBase::~XrImageTransportBase() = default;

void XrImageTransportBase::DestroySharedBuffers(WebXrPresentationState* webxr) {
  DVLOG(2) << __func__;
  CHECK(IsOnGlThread());

  if (!webxr || !UseSharedBuffer()) {
    return;
  }

  std::vector<std::unique_ptr<WebXrSharedBuffer>> buffers =
      webxr->TakeSharedBuffers();
  for (auto& buffer : buffers) {
    if (buffer->shared_image) {
      DCHECK(mailbox_bridge_);
      DVLOG(2) << ": DestroySharedImage, mailbox="
               << buffer->shared_image->mailbox().ToDebugString();
      // Note: the sync token may not be accurate. See comment in TransferFrame
      // below.
      mailbox_bridge_->DestroySharedImage(buffer->sync_token,
                                          std::move(buffer->shared_image));
    }
  }
}

void XrImageTransportBase::Initialize(WebXrPresentationState* webxr,
                                      XrInitStatusCallback callback) {
  CHECK(IsOnGlThread());
  DVLOG(2) << __func__;

  DoRuntimeInitialization(UseSharedBuffer() ? SharedBufferTextureTarget()
                                            : GL_TEXTURE_EXTERNAL_OES);

  if (UseSharedBuffer()) {
    DVLOG(2) << __func__ << ": UseSharedBuffer()=true";
  } else {
    DVLOG(2) << __func__ << ": UseSharedBuffer()=false, setting up surface";
    glGenTextures(1, &transport_texture_.id);

    // Transport Texture is bound to SurfaceTexture and must be TEXTURE_EXTERNAL
    transport_texture_.target = GL_TEXTURE_EXTERNAL_OES;
    transport_surface_texture_ =
        gl::SurfaceTexture::Create(transport_texture_.id);
    surface_size_ = {0, 0};
    mailbox_bridge_->CreateSurface(transport_surface_texture_.get());
    transport_surface_texture_->SetFrameAvailableCallback(
        base::BindRepeating(&XrImageTransportBase::OnFrameAvailable,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  mailbox_bridge_->CreateAndBindContextProvider(
      base::BindOnce(&XrImageTransportBase::OnMailboxBridgeReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void XrImageTransportBase::OnMailboxBridgeReady(XrInitStatusCallback callback) {
  DVLOG(2) << __func__;
  CHECK(IsOnGlThread());

  DCHECK(mailbox_bridge_->IsConnected());

  bool success = true;

  // DISABLE_RENDERING_TO_RGB_EXTERNAL_TEXTURE is needed because some drivers
  // don't allow rendering to TEXTURE_EXTERNAL, it's not applicable if we use
  // TEXTURE_2D.
  if (!base::FeatureList::IsEnabled(
          features::kUseTargetTexture2DForSharedBuffers) &&
      UseSharedBuffer()) {
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
      XrImageTransportBase::disable_shared_buffer_ = true;
      DCHECK(!UseSharedBuffer());
      success = false;
    }
  }
  std::move(callback).Run(success);
}

void XrImageTransportBase::SetFrameAvailableCallback(
    XrFrameCallback on_transport_frame_available) {
  CHECK(IsOnGlThread());
  DVLOG(2) << __func__;
  on_transport_frame_available_ = std::move(on_transport_frame_available);
}

void XrImageTransportBase::OnFrameAvailable() {
  DVLOG(2) << __func__;
  DCHECK(on_transport_frame_available_);

  // This function assumes that there's only at most one frame in "processing"
  // state at any given time, the webxr_ state handling ensures that. Drawing
  // and swapping twice without an intervening UpdateTexImage call would lose
  // an image, and that would lead to images and poses getting out of sync.
  //
  // It also assumes that the XrImageTransportBase and Surface only exist for
  // the duration of a single session, and a new session will use fresh objects.
  // For comparison, see GvrSchedulerDelegate::OnWebXrFrameAvailable which has
  // more complex logic to support a lifetime across multiple sessions,
  // including handling a possibly-unconsumed frame left over from a previous
  // session.

  transport_surface_texture_->UpdateTexImage();

  // The SurfaceTexture needs to be drawn using the corresponding
  // UV transform, that's usually a Y flip.
  transport_surface_texture_->GetTransformMatrix(
      transport_surface_texture_uv_matrix_);
  transport_surface_texture_uv_transform_ =
      gfx::Transform::ColMajorF(transport_surface_texture_uv_matrix_);

  on_transport_frame_available_.Run(transport_surface_texture_uv_transform_);
}

bool XrImageTransportBase::ResizeSharedBuffer(WebXrPresentationState* webxr,
                                              const gfx::Size& size,
                                              WebXrSharedBuffer* buffer) {
  CHECK(IsOnGlThread());

  if (buffer->size == size) {
    return false;
  }

  TRACE_EVENT0("gpu", __func__);
  // Unbind previous image (if any).
  if (buffer->shared_image) {
    DVLOG(2) << ": DestroySharedImage, mailbox="
             << buffer->shared_image->mailbox().ToDebugString();
    // Note: the sync token may not be accurate. See comment in TransferFrame
    // below.
    mailbox_bridge_->DestroySharedImage(buffer->sync_token,
                                        std::move(buffer->shared_image));
  }

  DVLOG(2) << __func__ << ": width=" << size.width()
           << " height=" << size.height();
  // Remove reference to previous image (if any).
  buffer->local_eglimage.reset();

  static constexpr gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  static constexpr gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  // The SharedImages created here will eventually be transferred to other
  // processes to have their contents read/written via WebGL for WebXR.
  gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;

  // Create a new AHardwareBuffer backed handle.
  buffer->scoped_ahb_handle =
      gpu::CreateScopedHardwareBufferHandle(size, format, usage);

  // Create a GMB Handle from AHardwareBuffer handle.
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::ANDROID_HARDWARE_BUFFER;
  // GpuMemoryBufferId is not used in this case and hence hardcoding it to 1
  // here.
  gmb_handle.id = gfx::GpuMemoryBufferId(1);
  gmb_handle.android_hardware_buffer = buffer->scoped_ahb_handle.Clone();

  buffer->shared_image = mailbox_bridge_->CreateSharedImage(
      std::move(gmb_handle), format, size, gfx::ColorSpace(),
      shared_image_usage, buffer->sync_token);
  CHECK(buffer->shared_image);

  DVLOG(2) << ": CreateSharedImage, mailbox="
           << buffer->shared_image->mailbox().ToDebugString()
           << ", SyncToken=" << buffer->sync_token.ToDebugString();

  // Create an EGLImage for the buffer.
  auto egl_image =
      gpu::CreateEGLImageFromAHardwareBuffer(buffer->scoped_ahb_handle.get());
  if (!egl_image.is_valid()) {
    DLOG(WARNING) << __func__ << ": ERROR: failed to initialize image!";
    return false;
  }

  glBindTexture(buffer->local_texture.target, buffer->local_texture.id);
  glTexParameteri(buffer->local_texture.target, GL_TEXTURE_WRAP_S,
                  GL_CLAMP_TO_EDGE);
  glTexParameteri(buffer->local_texture.target, GL_TEXTURE_WRAP_T,
                  GL_CLAMP_TO_EDGE);
  glTexParameteri(buffer->local_texture.target, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR);
  glTexParameteri(buffer->local_texture.target, GL_TEXTURE_MAG_FILTER,
                  GL_LINEAR);
  glEGLImageTargetTexture2DOES(buffer->local_texture.target, egl_image.get());
  buffer->local_eglimage = std::move(egl_image);

  // Save size to avoid resize next time.
  DVLOG(1) << __func__ << ": resized to " << size.width() << "x"
           << size.height();
  buffer->size = size;
  return true;
}

std::unique_ptr<WebXrSharedBuffer> XrImageTransportBase::CreateBuffer() {
  CHECK(IsOnGlThread());
  std::unique_ptr<WebXrSharedBuffer> buffer =
      std::make_unique<WebXrSharedBuffer>();
  // Local resources
  glGenTextures(1, &buffer->local_texture.id);
  buffer->local_texture.target = SharedBufferTextureTarget();
  return buffer;
}

WebXrSharedBuffer* XrImageTransportBase::TransferFrame(
    WebXrPresentationState* webxr,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  CHECK(IsOnGlThread());
  CHECK(UseSharedBuffer());

  if (!webxr->GetAnimatingFrame()->shared_buffer) {
    webxr->GetAnimatingFrame()->shared_buffer = CreateBuffer();
  }

  WebXrSharedBuffer* shared_buffer =
      webxr->GetAnimatingFrame()->shared_buffer.get();
  ResizeSharedBuffer(webxr, frame_size, shared_buffer);
  // Sanity check that the lazily created/resized buffer looks valid.
  DCHECK(shared_buffer->shared_image);
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
  DCHECK(shared_buffer->sync_token.HasData());
  DVLOG(2) << ": SyncToken=" << shared_buffer->sync_token.ToDebugString();

  return shared_buffer;
}

void XrImageTransportBase::CreateGpuFenceForSyncToken(
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  CHECK(IsOnGlThread());
  DVLOG(2) << __func__;
  mailbox_bridge_->CreateGpuFence(sync_token, std::move(callback));
}

void XrImageTransportBase::WaitSyncToken(const gpu::SyncToken& sync_token) {
  CHECK(IsOnGlThread());
  mailbox_bridge_->WaitSyncToken(sync_token);
}

void XrImageTransportBase::ServerWaitForGpuFence(
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  CHECK(IsOnGlThread());
  std::unique_ptr<gl::GLFence> local_fence =
      gl::GLFence::CreateFromGpuFence(*gpu_fence);
  local_fence->ServerWait();
}

LocalTexture XrImageTransportBase::GetRenderingTexture(
    WebXrPresentationState* webxr) {
  CHECK(IsOnGlThread());
  if (UseSharedBuffer()) {
    return webxr->GetRenderingFrame()->shared_buffer->local_texture;
  } else {
    return transport_texture_;
  }
}

void XrImageTransportBase::CopyMailboxToSurfaceAndSwap(
    const gfx::Size& frame_size,
    const gpu::MailboxHolder& mailbox,
    const gfx::Transform& uv_transform) {
  CHECK(IsOnGlThread());
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

bool XrImageTransportBase::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

}  // namespace device
