// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CAMERA_UPDATE_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CAMERA_UPDATE_HELPER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"

namespace blink {

class WebGLRenderingContextBase;
class WebGLTexture;
class XRSession;

class XRCameraUpdateHelper final
    : public GarbageCollected<XRCameraUpdateHelper>,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver {
  USING_PRE_FINALIZER(XRCameraUpdateHelper, Dispose);

 public:
  XRCameraUpdateHelper(XRSession* session,
                       WebGLRenderingContextBase* webgl_context);
  ~XRCameraUpdateHelper() override;

  void Dispose();

  WebGLTexture* GetCameraTexture();
  gpu::SyncToken OnFrameEnd();

  // WebGraphicsContext3DProviderWrapper::DestructionObserver:
  void OnContextDestroyed() override;

  void Trace(Visitor*) const;

 private:
  void MaybeAccessCameraTexture();
  gpu::SyncToken EndCameraTextureAccess();

  Member<XRSession> session_;
  Member<WebGLRenderingContextBase> webgl_context_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  std::unique_ptr<gpu::SharedImageTexture> camera_image_shared_image_texture_;
  std::unique_ptr<gpu::SharedImageTexture::ScopedAccess>
      camera_image_texture_scoped_access_;
  gpu::SyncToken pending_camera_sync_token_;

  bool did_try_to_access_camera_texture_ = false;
  // WebGL texture that points to the |camera_image_texture_|. Must be
  // notified via a call to |WebGLUnownedTexture::OnGLDeleteTextures()| when
  // |camera_image_texture_id_| is deleted.
  Member<WebGLUnownedTexture> camera_image_texture_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CAMERA_UPDATE_HELPER_H_
