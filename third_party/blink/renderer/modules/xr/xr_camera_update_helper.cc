// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_camera_update_helper.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_shared_image_manager.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRCameraUpdateHelper::XRCameraUpdateHelper(
    XRSession* session,
    WebGLRenderingContextBase* webgl_context)
    : session_(session), webgl_context_(webgl_context) {}

WebGLTexture* XRCameraUpdateHelper::GetCameraTexture() {
  DVLOG(1) << __func__;

  // We already have a WebGL texture for the camera image - return it:
  if (camera_image_texture_) {
    return camera_image_texture_.Get();
  }

  // Lazily try to get the WebGL texture.
  MaybeAccessCameraTexture();

  // We don't have a WebGL texture, and we cannot create it - return null:
  if (!camera_image_shared_image_texture_) {
    return nullptr;
  }

  // We don't have a WebGL texture, but we can create it, so create, store and
  // return it:
  camera_image_texture_ = MakeGarbageCollected<WebGLUnownedTexture>(
      webgl_context_, camera_image_shared_image_texture_->id(), GL_TEXTURE_2D);

  return camera_image_texture_.Get();
}

void XRCameraUpdateHelper::MaybeAccessCameraTexture() {
  // This is cleared in `OnFrameEnd`, and helps ensure that if we fail we only
  // query for the texture once per frame.
  if (did_try_to_access_camera_texture_) {
    return;
  }
  did_try_to_access_camera_texture_ = true;

  const XRSharedImageData& camera_image_data =
      session_->LayerSharedImageManager().CameraSharedImage();

  if (camera_image_data.shared_image) {
    DVLOG(3) << __func__ << ": camera_image_data.shared_image->mailbox()"
             << camera_image_data.shared_image->mailbox().ToDebugString();
    scoped_refptr<gpu::ClientSharedImage> buffer_shared_image =
        camera_image_data.shared_image;
    gpu::SyncToken buffer_sync_token = camera_image_data.sync_token;
    gpu::gles2::GLES2Interface* gl =
        webgl_context_->GetDrawingBuffer()->ContextGL();

    DVLOG(3) << __func__
             << ": buffer_sync_token=" << buffer_sync_token.ToDebugString();
    if (buffer_shared_image) {
      camera_image_shared_image_texture_ =
          buffer_shared_image->CreateGLTexture(gl);
      DVLOG(3) << __func__ << ": camera_image_shared_image_texture_->id()="
               << camera_image_shared_image_texture_->id();
      uint32_t texture_target = buffer_shared_image->GetTextureTarget();
      camera_image_texture_scoped_access_ =
          camera_image_shared_image_texture_->BeginAccess(buffer_sync_token,
                                                          /*readonly=*/true);
      gl->BindTexture(texture_target,
                      camera_image_texture_scoped_access_->texture_id());
    }
  }
}

gpu::SyncToken XRCameraUpdateHelper::OnFrameEnd() {
  did_try_to_access_camera_texture_ = false;
  gpu::SyncToken sync_token;

  // The session_ might have ended in the middle of the frame. Only perform the
  // main work of OnFrameEnd if it's still valid. Otherwise, simply ensure the
  // shared image access is properly ended.
  if (session_->ended()) {
    if (camera_image_texture_scoped_access_) {
      sync_token = gpu::SharedImageTexture::ScopedAccess::EndAccess(
          std::move(camera_image_texture_scoped_access_));
      camera_image_shared_image_texture_.reset();
    }
    return sync_token;
  }

  if (session_->immersive()) {
    // Need to stop accessing the camera image texture before calling
    // `SubmitLayer` so that we stop using it before the sync token
    // that `SubmitLayer` will generate.
    if (camera_image_shared_image_texture_) {
      const XRSharedImageData& camera_image_data =
          session_->LayerSharedImageManager().CameraSharedImage();

      // We shouldn't ever have a camera texture if the holder wasn't present:
      CHECK(camera_image_data.shared_image);

      DVLOG(3) << __func__
               << ": deleting camera image texture, "
                  "camera_image_shared_image_texture_->id()="
               << camera_image_shared_image_texture_->id();

      sync_token = gpu::SharedImageTexture::ScopedAccess::EndAccess(
          std::move(camera_image_texture_scoped_access_));
      camera_image_shared_image_texture_.reset();

      // Notify our WebGLUnownedTexture (created from
      // camera_image_shared_image_texture_) that we have deleted it. Also,
      // release the reference since we no longer need it (note that it could
      // still be kept alive by the JS application, but should be a defunct
      // object).
      if (camera_image_texture_) {
        camera_image_texture_->OnGLDeleteTextures();
        camera_image_texture_ = nullptr;
      }
    }
  }
  return sync_token;
}

void XRCameraUpdateHelper::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(webgl_context_);
  visitor->Trace(camera_image_texture_);
}

}  // namespace blink
