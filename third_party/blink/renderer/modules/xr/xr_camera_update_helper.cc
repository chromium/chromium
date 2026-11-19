// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_camera_update_helper.h"

#include <utility>

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_shared_image_manager.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

namespace blink {

XRCameraUpdateHelper::XRCameraUpdateHelper(
    XRSession* session,
    WebGLRenderingContextBase* webgl_context)
    : session_(session), webgl_context_(webgl_context) {}

XRCameraUpdateHelper::~XRCameraUpdateHelper() = default;

void XRCameraUpdateHelper::Dispose() {
  EndCameraTextureAccess();
}

WebGLTexture* XRCameraUpdateHelper::GetCameraTexture() {
  DVLOG(1) << __func__;

  if (!webgl_context_ || webgl_context_->isContextLost()) {
    return nullptr;
  }

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

  if (!webgl_context_ || webgl_context_->isContextLost()) {
    return;
  }

  DrawingBuffer* drawing_buffer = webgl_context_->GetDrawingBuffer();
  if (!drawing_buffer || drawing_buffer->destroyed()) {
    return;
  }

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      drawing_buffer->ContextProviderWeakPtr();
  if (!context_provider_wrapper) {
    return;
  }

  gpu::gles2::GLES2Interface* gl =
      context_provider_wrapper->ContextProvider().ContextGL();
  if (!gl) {
    return;
  }

  const XRSharedImageData& camera_image_data =
      session_->LayerSharedImageManager().CameraSharedImage();

  if (camera_image_data.shared_image) {
    DVLOG(3) << __func__ << ": camera_image_data.shared_image->mailbox()"
             << camera_image_data.shared_image->mailbox().ToDebugString();
    scoped_refptr<gpu::ClientSharedImage> buffer_shared_image =
        camera_image_data.shared_image;
    gpu::SyncToken buffer_sync_token = camera_image_data.sync_token;

    DVLOG(3) << __func__
             << ": buffer_sync_token=" << buffer_sync_token.ToDebugString();
    CHECK(!context_provider_wrapper_);
    CHECK(!camera_image_shared_image_texture_);
    CHECK(!camera_image_texture_scoped_access_);

    context_provider_wrapper_ = std::move(context_provider_wrapper);
    context_provider_wrapper_->AddObserver(this);

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

gpu::SyncToken XRCameraUpdateHelper::EndCameraTextureAccess() {
  gpu::SyncToken sync_token;

  if (camera_image_texture_scoped_access_) {
    sync_token = gpu::SharedImageTexture::ScopedAccess::EndAccess(
        std::move(camera_image_texture_scoped_access_));
    if (sync_token.HasData() && context_provider_wrapper_) {
      if (gpu::gles2::GLES2Interface* gl =
              context_provider_wrapper_->ContextProvider().ContextGL()) {
        int8_t* sync_token_data = sync_token.GetData();
        gl->VerifySyncTokensCHROMIUM(&sync_token_data, 1);
      }
    }
  }

  if (camera_image_shared_image_texture_) {
    DVLOG(3) << __func__
             << ": deleting camera image texture, "
                "camera_image_shared_image_texture_->id()="
             << camera_image_shared_image_texture_->id();
    camera_image_shared_image_texture_.reset();
  }

  if (context_provider_wrapper_) {
    context_provider_wrapper_->RemoveObserver(this);
    context_provider_wrapper_.reset();
  }

  // Notify our WebGLUnownedTexture (created from
  // camera_image_shared_image_texture_) that we have deleted it. Also,
  // release the reference since we no longer need it (note that it could
  // still be kept alive by the JS application, but should be a defunct
  // object).
  if (camera_image_texture_) {
    camera_image_texture_->OnGLDeleteTextures();
    camera_image_texture_ = nullptr;
  }

  return sync_token;
}

void XRCameraUpdateHelper::OnContextDestroyed() {
  gpu::SyncToken sync_token = EndCameraTextureAccess();
  if (sync_token.HasData()) {
    pending_camera_sync_token_ = sync_token;
  }
}

gpu::SyncToken XRCameraUpdateHelper::OnFrameEnd() {
  did_try_to_access_camera_texture_ = false;
  gpu::SyncToken sync_token =
      std::exchange(pending_camera_sync_token_, gpu::SyncToken());

  // Need to stop accessing the camera image texture before calling
  // `SubmitFrame` so that we stop using it before the sync token
  // is exported back to the XR runtime.
  if (camera_image_shared_image_texture_ ||
      camera_image_texture_scoped_access_) {
    if (!session_->ended()) {
      const XRSharedImageData& camera_image_data =
          session_->LayerSharedImageManager().CameraSharedImage();

      // We shouldn't ever have a camera texture if the holder wasn't present:
      CHECK(camera_image_data.shared_image);
    }

    sync_token = EndCameraTextureAccess();
  }

  return sync_token;
}

void XRCameraUpdateHelper::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(webgl_context_);
  visitor->Trace(camera_image_texture_);
}

}  // namespace blink
