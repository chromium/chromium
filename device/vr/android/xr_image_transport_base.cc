// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/xr_image_transport_base.h"

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
#include "gpu/ipc/common/android/android_hardware_buffer_utils.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"

namespace device {

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

  if (!webxr) {
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
                                      XrInitStatusCallback callback,
                                      bool webgpu_session) {
  CHECK(IsOnGlThread());
  DVLOG(2) << __func__;

  webgpu_session_ = webgpu_session;

  DoRuntimeInitialization();

  mailbox_bridge_->CreateAndBindContextProvider(
      base::BindOnce(&XrImageTransportBase::OnMailboxBridgeReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void XrImageTransportBase::OnMailboxBridgeReady(XrInitStatusCallback callback) {
  DVLOG(2) << __func__;
  CHECK(IsOnGlThread());

  DCHECK(mailbox_bridge_->IsConnected());

  std::move(callback).Run(true);
}

bool XrImageTransportBase::ResizeSharedBuffer(WebXrPresentationState* webxr,
                                              const gfx::Size& size,
                                              WebXrSharedBuffer* buffer) {
  CHECK(IsOnGlThread());

  if (buffer->shared_image && buffer->shared_image->size() == size) {
    return false;
  }

  TRACE_EVENT0("gpu", "ResizeSharedBuffer");
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

  static constexpr viz::SharedImageFormat format =
      viz::SinglePlaneFormat::kRGBA_8888;
  static constexpr gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  // The SharedImages created here will eventually be transferred to other
  // processes to have their contents read/written via WebGL for WebXR.
  gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;

  // If the XRSession is producing frames with WebGPU then the appropriate usage
  // also needs to be added.
  if (IsWebGPUSession()) {
    shared_image_usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
                          gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE;
  }

  // Create a new AHardwareBuffer backed handle.
  buffer->scoped_ahb_handle =
      gpu::CreateScopedHardwareBufferHandle(size, format, usage);

  // Create a GMB Handle from AHardwareBuffer handle.
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::ANDROID_HARDWARE_BUFFER;
  gmb_handle.android_hardware_buffer = buffer->scoped_ahb_handle.Clone();

  buffer->shared_image = mailbox_bridge_->CreateSharedImage(
      std::move(gmb_handle), format, size, gfx::ColorSpace(),
      shared_image_usage, buffer->sync_token);
  CHECK(buffer->shared_image);

  DVLOG(2) << ": CreateSharedImage, mailbox="
           << buffer->shared_image->mailbox().ToDebugString()
           << ", SyncToken=" << buffer->sync_token.ToDebugString()
           << ", size=" << size.ToString();

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

  return true;
}

std::unique_ptr<WebXrSharedBuffer> XrImageTransportBase::CreateBuffer() {
  CHECK(IsOnGlThread());
  std::unique_ptr<WebXrSharedBuffer> buffer =
      std::make_unique<WebXrSharedBuffer>();
  // Local resources
  glGenTextures(1, &buffer->local_texture.id);
  buffer->local_texture.target = GL_TEXTURE_2D;
  return buffer;
}

WebXrSharedBuffer* XrImageTransportBase::TransferFrame(
    WebXrPresentationState* webxr,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  CHECK(IsOnGlThread());

  if (!webxr->GetAnimatingFrame()->shared_buffer) {
    webxr->GetAnimatingFrame()->shared_buffer = CreateBuffer();
  }

  WebXrSharedBuffer* shared_buffer =
      webxr->GetAnimatingFrame()->shared_buffer.get();
  ResizeSharedBuffer(webxr, frame_size, shared_buffer);
  // Sanity check that the lazily created/resized buffer looks valid.
  DCHECK(shared_buffer->shared_image);
  DCHECK(shared_buffer->local_eglimage.is_valid());
  DCHECK_EQ(shared_buffer->shared_image->size(), frame_size);

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
  return webxr->GetRenderingFrame()->shared_buffer->local_texture;
}

bool XrImageTransportBase::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

}  // namespace device
