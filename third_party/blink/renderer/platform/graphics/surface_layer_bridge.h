// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SURFACE_LAYER_BRIDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SURFACE_LAYER_BRIDGE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace cc {
class SolidColorLayer;
class SurfaceLayer;
}  // namespace cc

namespace blink {

// The SurfaceLayerBridge facilitates communication about changes to a Surface
// between the Render and Browser processes.
class PLATFORM_EXPORT SurfaceLayerBridge
    : public blink::mojom::blink::EmbeddedFrameSinkClient,
      public blink::mojom::blink::SurfaceEmbedder,
      public WebSurfaceLayerBridge {
 public:
  SurfaceLayerBridge(
      viz::FrameSinkId parent_frame_sink_id,
      WebSurfaceLayerBridgeObserver*,
      cc::UpdateSubmissionStateCB update_submission_state_callback);
  ~SurfaceLayerBridge() override;

  void CreateSolidColorLayer();

  // Implementation of blink::mojom::blink::EmbeddedFrameSinkClient
  void BindSurfaceEmbedder(
      mojo::PendingReceiver<mojom::blink::SurfaceEmbedder> receiver) override;

  void EmbedSurface(const viz::SurfaceId& surface_id);

  // Implementation of blink::mojom::blink::SurfaceEmbedder
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) override;

  // Implementation of WebSurfaceLayerBridge.
  cc::Layer* GetCcLayer() const override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  void SetContentsOpaque(bool) override;
  void CreateSurfaceLayer() override;
  void ClearObserver() override;

  const viz::SurfaceId& GetSurfaceId() const override {
    return current_surface_id_;
  }

  base::TimeTicks GetLocalSurfaceIdAllocationTime() const override;

 private:
  scoped_refptr<cc::SurfaceLayer> surface_layer_;
  scoped_refptr<cc::SolidColorLayer> solid_color_layer_;

  // The |observer_| handles unregistering the contents layer on its own.
  WebSurfaceLayerBridgeObserver* observer_;
  cc::UpdateSubmissionStateCB update_submission_state_callback_;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  mojo::Receiver<blink::mojom::blink::EmbeddedFrameSinkClient> receiver_{this};
  mojo::Receiver<blink::mojom::blink::SurfaceEmbedder>
      surface_embedder_receiver_{this};

  const viz::FrameSinkId frame_sink_id_;
  viz::SurfaceId current_surface_id_;
  const viz::FrameSinkId parent_frame_sink_id_;
  bool opaque_ = false;
  bool surface_activated_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SURFACE_LAYER_BRIDGE_H_
