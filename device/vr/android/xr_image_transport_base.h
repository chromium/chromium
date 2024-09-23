// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_XR_IMAGE_TRANSPORT_BASE_H_
#define DEVICE_VR_ANDROID_XR_IMAGE_TRANSPORT_BASE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/vr/android/local_texture.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/gl_bindings.h"

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

using XrFrameCallback = base::RepeatingCallback<void(const gfx::Transform&)>;
using XrInitStatusCallback = base::OnceCallback<void(bool success)>;

// This class handles transporting WebGL rendered output from the GPU process's
// command buffer GL context to the local GL context, and compositing WebGL
// output onto the camera image using the local GL context.
// Non pure-virtual methods are only intended to be overridden for testing
// purposes.
class XrImageTransportBase {
 public:
  // If true, use shared buffer transport aka DRAW_INTO_TEXTURE_MAILBOX.
  // If false, use Surface transport aka SUBMIT_AS_MAILBOX_HOLDER.
  static bool UseSharedBuffer();

  static GLuint SharedBufferTextureTarget();

  explicit XrImageTransportBase(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge);

  XrImageTransportBase(const XrImageTransportBase&) = delete;
  XrImageTransportBase& operator=(const XrImageTransportBase&) = delete;

  virtual ~XrImageTransportBase();

  virtual void DestroySharedBuffers(WebXrPresentationState* webxr);

  // All methods must be called on a valid GL thread. Initialization
  // must happen after the local GL context is ready for use. That
  // starts the asynchronous setup for the GPU process command buffer
  // GL context via MailboxToSurfaceBridge, and the callback is called
  // once that's complete.
  virtual void Initialize(WebXrPresentationState* webxr,
                          XrInitStatusCallback callback);

  // Only valid when using SharedBuffers, this ensures that the current
  // animating frame is populated with texture information for a valid and
  // correctly sized shared buffer backed by an EGL image. This function
  // intends to return to its caller a sync token as well as
  // a scoped_refptr<gpu::ClientSharedImage> pointing to this shared buffer
  // suitable to transfer to another process to allow it to write to the
  // shared buffer. The two values are currently returned together via
  // a wrapping WebXrSharedBuffer.
  // TODO(crbug.com/40286368): Change the return type to
  // scoped_refptr<gpu::ClientSharedImage> once the sync token is
  // incorporated into ClientSharedImage.
  virtual WebXrSharedBuffer* TransferFrame(WebXrPresentationState* webxr,
                                           const gfx::Size& frame_size,
                                           const gfx::Transform& uv_transform);
  virtual void CreateGpuFenceForSyncToken(
      const gpu::SyncToken& sync_token,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>);
  virtual void WaitSyncToken(const gpu::SyncToken& sync_token);
  virtual void CopyMailboxToSurfaceAndSwap(const gfx::Size& frame_size,
                                           const gpu::MailboxHolder& mailbox,
                                           const gfx::Transform& uv_transform);

  void SetFrameAvailableCallback(XrFrameCallback on_frame_available);
  void ServerWaitForGpuFence(std::unique_ptr<gfx::GpuFence> gpu_fence);

 protected:
  bool IsOnGlThread() const;

  virtual std::unique_ptr<WebXrSharedBuffer> CreateBuffer();

  // Returns true if the buffer was resized and its sync token updated.
  bool ResizeSharedBuffer(WebXrPresentationState* webxr,
                          const gfx::Size& size,
                          WebXrSharedBuffer* buffer);

  // This method provides an abstraction to the caller about whether the system
  // is running in SharedBuffer mode or not, and returns the texture that the
  // renderer has most recently populated and submitted back to the device for
  // rendering. There must be a texture in the `RenderingFrame` state of the
  // `WebXrPresentationState` machine for this to properly return data.
  LocalTexture GetRenderingTexture(WebXrPresentationState* webxr);

  // Runs before the rest of the initialization for the XrImageTransport to
  // allow for any specialized gl context setup or other setup that may be
  // needed by the particular runtime that's in use.
  virtual void DoRuntimeInitialization(int texture_target) = 0;

  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

 private:
  // Used to disable UseSharedBuffer on platforms where the feature is available
  // but unusable due to driver bugs. Must be mutable so that it can be switched
  // to true persistently before retrying session creation, so it can't be
  // constexpr or inline.
  static bool disable_shared_buffer_;

  void ResizeSurface(const gfx::Size& size);
  void OnMailboxBridgeReady(XrInitStatusCallback callback);
  void OnFrameAvailable();

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // Used for Surface transport (Android N)
  //
  // samplerExternalOES texture data for WebXR content image.
  LocalTexture transport_texture_;
  gfx::Size surface_size_;
  scoped_refptr<gl::SurfaceTexture> transport_surface_texture_;
  gfx::Transform transport_surface_texture_uv_transform_;
  float transport_surface_texture_uv_matrix_[16];
  XrFrameCallback on_transport_frame_available_;

  // Must be last.
  base::WeakPtrFactory<XrImageTransportBase> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_XR_IMAGE_TRANSPORT_BASE_H_
