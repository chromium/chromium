// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_AR_IMAGE_TRANSPORT_H_
#define DEVICE_VR_ANDROID_ARCORE_AR_IMAGE_TRANSPORT_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/vr/android/arcore/ar_renderer.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gl {
class SurfaceTexture;
}  // namespace gl

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gpu {
struct MailboxHolder;
struct SyncToken;
}  // namespace gpu

namespace device {

class MailboxToSurfaceBridge;
class WebXrPresentationState;
struct WebXrSharedBuffer;

using XrFrameCallback = base::RepeatingCallback<void(const gfx::Transform&)>;
using XrInitStatusCallback = base::OnceCallback<void(bool success)>;

// This class handles transporting WebGL rendered output from the GPU process's
// command buffer GL context to the local GL context, and compositing WebGL
// output onto the camera image using the local GL context.
class COMPONENT_EXPORT(VR_ARCORE) ArImageTransport {
 public:
  // If true, use shared buffer transport aka DRAW_INTO_TEXTURE_MAILBOX.
  // If false, use Surface transport aka SUBMIT_AS_MAILBOX_HOLDER.
  static bool UseSharedBuffer();

  explicit ArImageTransport(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge);

  ArImageTransport(const ArImageTransport&) = delete;
  ArImageTransport& operator=(const ArImageTransport&) = delete;

  virtual ~ArImageTransport();

  virtual void DestroySharedBuffers(WebXrPresentationState* webxr);

  // All methods must be called on a valid GL thread. Initialization
  // must happen after the local GL context is ready for use. That
  // starts the asynchronous setup for the GPU process command buffer
  // GL context via MailboxToSurfaceBridge, and the callback is called
  // once that's complete.
  virtual void Initialize(WebXrPresentationState* webxr,
                          XrInitStatusCallback callback);

  virtual GLuint GetCameraTextureId();

  // This creates a shared buffer if one doesn't already exist, and populates it
  // with the current animating frame's buffer data. It returns a
  // gpu::Mailboxholder with this shared buffer data.
  virtual gpu::MailboxHolder TransferFrame(WebXrPresentationState* webxr,
                                           const gfx::Size& frame_size,
                                           const gfx::Transform& uv_transform);

  // This transfers whatever the contents of the texture specified
  // by GetCameraTextureId() is at the time it is called and returns
  // a gpu::MailboxHolder with that texture copied to a shared buffer.
  virtual gpu::MailboxHolder TransferCameraImageFrame(
      WebXrPresentationState* webxr,
      const gfx::Size& frame_size,
      const gfx::Transform& uv_transform);

  virtual void CreateGpuFenceForSyncToken(
      const gpu::SyncToken& sync_token,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>);
  virtual void CopyCameraImageToFramebuffer(const gfx::Size& frame_size,
                                            const gfx::Transform& uv_transform);
  virtual void CopyDrawnImageToFramebuffer(WebXrPresentationState* webxr,
                                           const gfx::Size& frame_size,
                                           const gfx::Transform& uv_transform);
  virtual void CopyTextureToFramebuffer(GLuint texture,
                                        const gfx::Size& frame_size,
                                        const gfx::Transform& uv_transform);
  virtual void WaitSyncToken(const gpu::SyncToken& sync_token);
  virtual void CopyMailboxToSurfaceAndSwap(const gfx::Size& frame_size,
                                           const gpu::MailboxHolder& mailbox,
                                           const gfx::Transform& uv_transform);

  void SetFrameAvailableCallback(XrFrameCallback on_frame_available);
  void ServerWaitForGpuFence(std::unique_ptr<gfx::GpuFence> gpu_fence);

 private:
  // Used to disable UseSharedBuffer on platforms where the feature is available
  // but unusable due to driver bugs. Must be mutable so that it can be switched
  // to true persistently before retrying session creation, so it can't be
  // constexpr or inline.
  static bool disable_shared_buffer_;

  std::unique_ptr<WebXrSharedBuffer> CreateBuffer();
  // Returns true if the buffer was resized and its sync token updated.
  bool ResizeSharedBuffer(WebXrPresentationState* webxr,
                          const gfx::Size& size,
                          WebXrSharedBuffer* buffer);
  void ResizeSurface(const gfx::Size& size);
  bool IsOnGlThread() const;
  void OnMailboxBridgeReady(XrInitStatusCallback callback);
  void OnFrameAvailable();
  std::unique_ptr<ArRenderer> ar_renderer_;
  // samplerExternalOES texture for the camera image.
  GLuint camera_texture_id_arcore_ = 0;
  GLuint camera_fbo_ = 0;
  GLuint camera_image_fbo_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

  // Used to limit framebuffer complete check to occurring once, due to it being
  // expensive.
  bool framebuffer_complete_checked_for_camera_buffer_ = false;

  // Used for Surface transport (Android N)
  //
  // samplerExternalOES texture data for WebXR content image.
  GLuint transport_texture_id_ = 0;
  gfx::Size surface_size_;
  scoped_refptr<gl::SurfaceTexture> transport_surface_texture_;
  gfx::Transform transport_surface_texture_uv_transform_;
  float transport_surface_texture_uv_matrix_[16];
  XrFrameCallback on_transport_frame_available_;

  // Must be last.
  base::WeakPtrFactory<ArImageTransport> weak_ptr_factory_{this};
};

class COMPONENT_EXPORT(VR_ARCORE) ArImageTransportFactory {
 public:
  virtual ~ArImageTransportFactory() = default;
  virtual std::unique_ptr<ArImageTransport> Create(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_AR_IMAGE_TRANSPORT_H_
