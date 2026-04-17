// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "third_party/blink/renderer/modules/xr/xr_render_state.h"

#include <algorithm>
#include <cmath>
#include <ranges>

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_render_state_init.h"
#include "third_party/blink/renderer/modules/xr/xr_composition_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
namespace blink {

namespace {
// The WebXR spec specifies that the min and max are up the UA, but have to be
// within 0 and Pi.  Using those exact numbers can lead to floating point math
// errors, so set them slightly inside those numbers.
constexpr double kMinFieldOfView = 0.01;
constexpr double kMaxFieldOfView = 3.13;
constexpr double kDefaultFieldOfView = M_PI * 0.5;
}  // namespace

XRRenderState::XRCameraUpdateHelper::XRCameraUpdateHelper(
    WebGLRenderingContextBase* webgl_context)
    : webgl_context_(webgl_context) {}

WebGLTexture* XRRenderState::XRCameraUpdateHelper::GetCameraTexture() {
  DVLOG(1) << __func__;

  // We already have a WebGL texture for the camera image - return it:
  if (camera_image_texture_) {
    return camera_image_texture_.Get();
  }

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

void XRRenderState::XRCameraUpdateHelper::OnFrameStart(XRSession* session) {
  const XRSharedImageData& camera_image_data =
      session->LayerSharedImageManager().CameraSharedImage();

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
    camera_image_shared_image_texture_ =
        buffer_shared_image->CreateGLTexture(gl);
    DVLOG(3) << __func__ << ": camera_image_shared_image_texture_->id()="
             << camera_image_shared_image_texture_->id();
    if (buffer_shared_image) {
      uint32_t texture_target = buffer_shared_image->GetTextureTarget();
      camera_image_texture_scoped_access_ =
          camera_image_shared_image_texture_->BeginAccess(buffer_sync_token,
                                                          /*readonly=*/true);
      gl->BindTexture(texture_target,
                      camera_image_texture_scoped_access_->texture_id());
    }
  }
}

void XRRenderState::XRCameraUpdateHelper::OnFrameEnd(XRSession* session) {
  // The session might have ended in the middle of the frame. Only perform the
  // main work of OnFrameEnd if it's still valid. Otherwise, simply ensure the
  // shared image access is properly ended.
  if (session->ended()) {
    if (camera_image_texture_scoped_access_) {
      gpu::SharedImageTexture::ScopedAccess::EndAccess(
          std::move(camera_image_texture_scoped_access_));
      camera_image_shared_image_texture_.reset();
    }
    return;
  }

  if (session->immersive()) {
    // Need to stop accessing the camera image texture before calling
    // `SubmitLayer` so that we stop using it before the sync token
    // that `SubmitLayer` will generate.
    if (camera_image_shared_image_texture_) {
      const XRSharedImageData& camera_image_data =
          session->LayerSharedImageManager().CameraSharedImage();

      // We shouldn't ever have a camera texture if the holder wasn't present:
      CHECK(camera_image_data.shared_image);

      DVLOG(3) << __func__
               << ": deleting camera image texture, "
                  "camera_image_shared_image_texture_->id()="
               << camera_image_shared_image_texture_->id();

      gpu::SharedImageTexture::ScopedAccess::EndAccess(
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
}

void XRRenderState::XRCameraUpdateHelper::Trace(Visitor* visitor) const {
  visitor->Trace(webgl_context_);
  visitor->Trace(camera_image_texture_);
}

XRRenderState::XRRenderState(bool immersive) : immersive_(immersive) {
  if (!immersive_) {
    inline_vertical_fov_ = kDefaultFieldOfView;
  }
}

void XRRenderState::Update(const XRRenderStateInit* init) {
  if (init->hasDepthNear()) {
    depth_near_ = std::max(0.0, init->depthNear());
  }
  if (init->hasDepthFar()) {
    depth_far_ = std::max(0.0, init->depthFar());
  }
  camera_helper_ = nullptr;
  if (init->hasBaseLayer()) {
    needs_layers_update_ |= base_layer_ != init->baseLayer();
    base_layer_ = init->baseLayer();
    UpdateLayersState(MakeGarbageCollected<FrozenArray<XRLayer>>());
    if (base_layer_ && base_layer_->framebuffer()) {
      camera_helper_ = MakeGarbageCollected<XRCameraUpdateHelper>(
          base_layer_->GetWebGLContext());
    }
  }
  if (init->hasLayers()) {
    if (!init->layers() || init->layers()->size() != layers_->size() ||
        !std::equal(init->layers()->begin(), init->layers()->end(),
                    layers_->begin())) {
      needs_layers_update_ = true;
    }

    base_layer_ = nullptr;
    UpdateLayersState(init->layers()
                          ? MakeGarbageCollected<FrozenArray<XRLayer>>(
                                HeapVector<Member<XRLayer>>(*init->layers()))
                          : MakeGarbageCollected<FrozenArray<XRLayer>>());
  }
  if (init->hasInlineVerticalFieldOfView()) {
    double fov = init->inlineVerticalFieldOfView();

    // Clamp the value between our min and max.
    fov = std::max(kMinFieldOfView, fov);
    fov = std::min(kMaxFieldOfView, fov);
    inline_vertical_fov_ = fov;
  }
}

void XRRenderState::UpdateLayersState(FrozenArray<XRLayer>* layers) {
  auto update_layers_redraw_state = [](FrozenArray<XRLayer>& source_layers,
                                       const FrozenArray<XRLayer>& other_layers,
                                       bool needs_redraw) {
    std::ranges::for_each(source_layers, [&](auto& source_layer) {
      if (!std::ranges::contains(other_layers, source_layer)) {
        source_layer->SetNeedsRedraw(needs_redraw);
      }
    });
  };

  // Disable needs redraw state for removed layers.
  update_layers_redraw_state(*layers_, *layers, /*needs_redraw=*/false);
  // Enable needs redraw state for newly added layers.
  update_layers_redraw_state(*layers, *layers_, /*needs_redraw=*/true);

  layers_ = layers;
}

XRLayer* XRRenderState::GetFirstLayer() const {
  if (base_layer_) {
    return base_layer_.Get();
  }
  if (!layers_->empty()) {
    return layers_->at(0);
  }
  return nullptr;
}

WebGLTexture* XRRenderState::GetCameraTexture() {
  return camera_helper_->GetCameraTexture();
}

HTMLCanvasElement* XRRenderState::output_canvas() const {
  if (base_layer_) {
    return base_layer_->output_canvas();
  }
  return nullptr;
}

std::optional<double> XRRenderState::inlineVerticalFieldOfView() const {
  if (immersive_) {
    return std::nullopt;
  }
  return inline_vertical_fov_;
}

bool XRRenderState::HasActiveLayer() const {
  return base_layer_ || (layers_ && !layers_->empty());
}

bool XRRenderState::HasLayer(XRLayer* layer) const {
  if (!layer) {
    return false;
  }
  if (layer == base_layer_) {
    return true;
  }
  if (layers_) {
    return std::ranges::contains(*layers_, layer);
  }
  return false;
}

void XRRenderState::OnFrameStart() {
  if (camera_helper_) {
    camera_helper_->OnFrameStart(base_layer_->session());
  }

  if (base_layer_) {
    base_layer_->OnFrameStart();
  }

  if (layers_) {
    for (XRLayer* layer : *layers_) {
      layer->OnFrameStart();
    }
  }
}

void XRRenderState::OnFrameEnd() {
  if (camera_helper_) {
    camera_helper_->OnFrameEnd(base_layer_->session());
  }

  if (base_layer_) {
    base_layer_->OnFrameEndWithoutSubmit();
    base_layer_->SubmitLayer();
  }

  if (layers_) {
    for (XRLayer* layer : *layers_) {
      layer->OnFrameEnd();
    }
  }
}

void XRRenderState::OnResize() {
  if (base_layer_) {
    base_layer_->OnResize();
  }

  if (layers_) {
    for (XRLayer* layer : *layers_) {
      layer->OnResize();
    }
  }
}

XRFrameTransportDelegate* XRRenderState::GetTransportDelegate() {
  if (XRLayer* first_layer = GetFirstLayer(); first_layer) {
    return first_layer->LayerClient()->GetTransportDelegate();
  }
  return nullptr;
}

bool XRRenderState::NeedLayersUpdate() {
  return needs_layers_update_;
}

void XRRenderState::OnLayersUpdated() {
  needs_layers_update_ = false;
}

void XRRenderState::UpdateLayersBackend(
    device::mojom::blink::XRLayerManager* layer_manager) {
  if (!needs_layers_update_) {
    return;
  }

  needs_layers_update_ = false;
  if (!layer_manager) {
    return;
  }

  blink::Vector<device::LayerId> ids;
  if (base_layer_) {
    ids.push_back(base_layer_->layer_id());
  }

  if (layers_) {
    ids.reserve(layers_->size());
    for (XRLayer* layer : *layers_) {
      ids.push_back(layer->layer_id());
    }
  }
  layer_manager->SetEnabledCompositionLayers(std::move(ids));
}

void XRRenderState::MaybeDispatchRedrawEvents() {
  if (layers_) {
    for (XRLayer* layer : *layers_) {
      layer->MaybeDispatchRedrawEvent();
    }
  }
}

void XRRenderState::OnTransferComplete(
    const Vector<device::LayerId>& layer_ids) {
  if (base_layer_) {
    base_layer_->SetNeedsRedraw(false);
  }

  if (layers_) {
    for (XRLayer* layer : *layers_) {
      if (layer_ids.Contains(layer->layer_id())) {
        layer->SetNeedsRedraw(false);
      }
    }
  }
}

void XRRenderState::Trace(Visitor* visitor) const {
  visitor->Trace(base_layer_);
  visitor->Trace(layers_);
  visitor->Trace(camera_helper_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
