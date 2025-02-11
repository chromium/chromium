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

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gpu {
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
                          XrInitStatusCallback callback,
                          bool webgpu_session);

  // Indicates if the session uses WebGPU to produce it's frames. Does not
  // have any effect on the API used for compositing/display.
  bool IsWebGPUSession() { return webgpu_session_; }

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
  virtual void DoRuntimeInitialization() = 0;

  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

 private:
  void OnMailboxBridgeReady(XrInitStatusCallback callback);

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  bool webgpu_session_ = false;

  // Must be last.
  base::WeakPtrFactory<XrImageTransportBase> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_XR_IMAGE_TRANSPORT_BASE_H_
