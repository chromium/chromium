// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_AR_IMAGE_TRANSPORT_H_
#define DEVICE_VR_ANDROID_ARCORE_AR_IMAGE_TRANSPORT_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/android/arcore/ar_renderer.h"
#include "device/vr/android/xr_image_transport_base.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

class MailboxToSurfaceBridge;
class WebXrPresentationState;

// This class handles transporting WebGL rendered output from the GPU process's
// command buffer GL context to the local GL context, and compositing WebGL
// output onto the camera image using the local GL context.
class COMPONENT_EXPORT(VR_ARCORE) ArImageTransport
    : public XrImageTransportBase {
 public:
  explicit ArImageTransport(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge);

  ArImageTransport(const ArImageTransport&) = delete;
  ArImageTransport& operator=(const ArImageTransport&) = delete;

  ~ArImageTransport() override;

  virtual GLuint GetCameraTextureId();

  // This transfers whatever the contents of the texture specified
  // by GetCameraTextureId() is at the time it is called and returns
  // a gpu::MailboxHolder with that texture copied to a shared buffer.
  virtual gpu::MailboxHolder TransferCameraImageFrame(
      WebXrPresentationState* webxr,
      const gfx::Size& frame_size,
      const gfx::Transform& uv_transform);
  void CopyCameraImageToFramebuffer(GLuint framebuffer,
                                    const gfx::Size& frame_size,
                                    const gfx::Transform& uv_transform);
  void CopyDrawnImageToFramebuffer(WebXrPresentationState* webxr,
                                   GLuint framebuffer,
                                   const gfx::Size& frame_size,
                                   const gfx::Transform& uv_transform);

 private:
  void DoRuntimeInitialization() override;

  // Makes all the relevant GL calls to actually draw the texture for the
  // runtime, will operate on the supplied framebuffer.
  void CopyTextureToFramebuffer(GLuint texture,
                                GLuint framebuffer,
                                const gfx::Size& frame_size,
                                const gfx::Transform& uv_transform);

  std::unique_ptr<ArRenderer> ar_renderer_;
  // samplerExternalOES texture for the camera image.
  GLuint camera_texture_id_arcore_ = 0;
  GLuint camera_fbo_ = 0;
  GLuint camera_image_fbo_ = 0;

#if DCHECK_IS_ON()
  // Used to limit framebuffer complete check to occurring once, due to it being
  // expensive.
  bool framebuffer_complete_checked_for_camera_buffer_ = false;
#endif

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
