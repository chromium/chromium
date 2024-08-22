// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/ar_image_transport.h"

#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/android/xr_image_transport_base.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"

namespace device {
ArImageTransport::ArImageTransport(
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge)
    : XrImageTransportBase(std::move(mailbox_bridge)) {
  DVLOG(2) << __func__;
}

ArImageTransport::~ArImageTransport() = default;

void ArImageTransport::DoRuntimeInitialization(int texture_taget) {
  renderer_ = std::make_unique<XrRenderer>();
  glGenTextures(1, &camera_texture_arcore_.id);
  camera_texture_arcore_.target = GL_TEXTURE_EXTERNAL_OES;

  glGenFramebuffersEXT(1, &camera_fbo_);
}

GLuint ArImageTransport::GetCameraTextureId() {
  return camera_texture_arcore_.id;
}

WebXrSharedBuffer* ArImageTransport::TransferCameraImageFrame(
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
    DCHECK(camera_image_shared_buffer->sync_token.HasData());
    WaitSyncToken(camera_image_shared_buffer->sync_token);
    DVLOG(3) << __func__
             << ": "
                "camera_image_shared_buffer->sync_"
                "token="
             << camera_image_shared_buffer->sync_token.ToDebugString();
  }
  // Sanity checks for the camera image buffer.
  DCHECK(camera_image_shared_buffer->shared_image);
  DCHECK(camera_image_shared_buffer->local_eglimage.is_valid());
  DCHECK_EQ(camera_image_shared_buffer->size, frame_size);

  // Temporarily change drawing buffer to the camera image buffer.
  if (!camera_image_fbo_) {
    glGenFramebuffersEXT(1, &camera_image_fbo_);
  }
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, camera_image_fbo_);
  glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            camera_image_shared_buffer->local_texture.target,
                            camera_image_shared_buffer->local_texture.id, 0);

  CopyCameraImageToFramebuffer(camera_image_fbo_, frame_size, uv_transform);

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

  mailbox_bridge_->GenSyncToken(&camera_image_shared_buffer->sync_token);
  DVLOG(3) << __func__ << ": camera_image_shared_buffer->sync_token="
           << camera_image_shared_buffer->sync_token.ToDebugString();

  return camera_image_shared_buffer;
}

void ArImageTransport::CopyCameraImageToFramebuffer(
    GLuint framebuffer,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  glDisable(GL_BLEND);
  CopyTextureToFramebuffer(camera_texture_arcore_, framebuffer, frame_size,
                           uv_transform);
}

void ArImageTransport::CopyDrawnImageToFramebuffer(
    WebXrPresentationState* webxr,
    GLuint framebuffer,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;

  // Set the blend mode for combining the drawn image (source) with the camera
  // image (destination). WebXR assumes that the canvas has premultiplied alpha,
  // so the source blend function is GL_ONE. The destination blend function is
  // (1 - src_alpha) as usual. (Setting that to GL_ONE would simulate an
  // additive AR headset that can't draw opaque black.)
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  auto texture = GetRenderingTexture(webxr);
  CopyTextureToFramebuffer(texture, framebuffer, frame_size, uv_transform);
}

void ArImageTransport::CopyTextureToFramebuffer(
    const LocalTexture& texture,
    GLuint framebuffer,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  // It would be a bit more efficient to do this one time on initialization,
  // but that would only be safe if ARCore and `XrRenderer` were guaranteed to
  // not modify these states. For now, keep the redundant operations to avoid
  // potential hard-to-find bugs.
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glViewport(0, 0, frame_size.width(), frame_size.height());

  // Draw the ARCore texture!
  float uv_transform_floats[16];
  uv_transform.GetColMajorF(uv_transform_floats);

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, framebuffer);
  renderer_->Draw(texture, uv_transform_floats);
}

std::unique_ptr<ArImageTransport> ArImageTransportFactory::Create(
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge) {
  return std::make_unique<ArImageTransport>(std::move(mailbox_bridge));
}

}  // namespace device
