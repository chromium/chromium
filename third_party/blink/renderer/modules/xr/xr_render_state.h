// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RENDER_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RENDER_STATE_H_

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/modules/xr/xr_layer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class HTMLCanvasElement;
class XRWebGLLayer;
class XRLayer;
class XRRenderStateInit;
class XRFrameTransportDelegate;

class XRRenderState : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRRenderState(bool immersive);
  ~XRRenderState() override = default;

  // Near and far depths are used when computing projection matrices for the
  // Session's views.
  double depthNear() const { return depth_near_; }
  double depthFar() const { return depth_far_; }
  std::optional<double> inlineVerticalFieldOfView() const;
  XRWebGLLayer* baseLayer() const { return base_layer_.Get(); }
  const FrozenArray<XRLayer>& layers() const { return *layers_.Get(); }

  // Returns either baseLayer or layers[0], or nullptr if neither is set.
  XRLayer* GetFirstLayer() const;

  HTMLCanvasElement* output_canvas() const;

  // Returns whether the layer composition sequence was modified by the last
  // call to Update().
  bool should_update_layers_backend() const { return needs_layers_update_; }

  void Update(const XRRenderStateInit* init);

  // Returns true if any layer was updated since the last status read.
  bool NeedLayersUpdate();
  void OnLayersUpdated();
  // Updates the mojom backend with the new layer composition sequence.
  void UpdateLayersBackend(device::mojom::blink::XRLayerManager*);

  // Calls OnFrameStart for each active layer.
  void OnFrameStart();
  // Calls OnFrameEnd for each active layer.
  void OnFrameEnd();
  // Calls OnResize for each active layer.
  void OnResize();
  // Dispatch "redraw" event for each active layer if needed.
  void MaybeDispatchRedrawEvents();

  // Only used when removing an outputContext from the session because it was
  // bound to a different session.
  void removeOutputContext();

  // Returns true if the current render state has at least one layer configured
  // for drawing. This could be either the baseLayer or a non-empty layers
  // list.
  bool HasActiveLayer() const;

  // Gets the transport delegate from the baseLayer, or from the last layer in
  // the layers list if baseLayer is null.
  XRFrameTransportDelegate* GetTransportDelegate();

  void Trace(Visitor*) const override;

 private:
  // Helper method to update the list of layers according to a new render state.
  // It also adds the needs redraw state for newly added layers and resets the
  // needs redraw state for removed layers.
  void UpdateLayersState(FrozenArray<XRLayer>* layers);

  bool immersive_;
  double depth_near_ = 0.1;
  double depth_far_ = 1000.0;
  Member<XRWebGLLayer> base_layer_;
  Member<FrozenArray<XRLayer>> layers_ =
      MakeGarbageCollected<FrozenArray<XRLayer>>();
  bool needs_layers_update_ = false;
  std::optional<double> inline_vertical_fov_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RENDER_STATE_H_
