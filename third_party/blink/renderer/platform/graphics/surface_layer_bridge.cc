// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/surface_layer_bridge.h"

#include <utility>

#include "base/feature_list.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

SurfaceLayerBridge::SurfaceLayerBridge(
    WebLayerTreeView* layer_tree_view,
    WebSurfaceLayerBridgeObserver* observer,
    cc::UpdateSubmissionStateCB update_submission_state_callback)
    : observer_(observer),
      update_submission_state_callback_(
          std::move(update_submission_state_callback)),
      binding_(this),
      frame_sink_id_(Platform::Current()->GenerateFrameSinkId()),
      parent_frame_sink_id_(layer_tree_view ? layer_tree_view->GetFrameSinkId()
                                            : viz::FrameSinkId()) {
  mojom::blink::EmbeddedFrameSinkProviderPtr provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&provider));
  // TODO(xlai): Ensure OffscreenCanvas commit() is still functional when a
  // frame-less HTML canvas's document is reparenting under another frame.
  // See crbug.com/683172.
  blink::mojom::blink::EmbeddedFrameSinkClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  provider->RegisterEmbeddedFrameSink(parent_frame_sink_id_, frame_sink_id_,
                                      std::move(client));
}

SurfaceLayerBridge::~SurfaceLayerBridge() = default;

void SurfaceLayerBridge::CreateSolidColorLayer() {
  // TODO(lethalantidote): Remove this logic. It should be covered by setting
  // the layer's opacity to false.
  solid_color_layer_ = cc::SolidColorLayer::Create();
  solid_color_layer_->SetBackgroundColor(SK_ColorTRANSPARENT);
  if (observer_)
    observer_->RegisterContentsLayer(solid_color_layer_.get());
}

void SurfaceLayerBridge::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  DCHECK(surface_info.is_valid());

  surface_activated_ = true;
  if (solid_color_layer_) {
    if (observer_)
      observer_->UnregisterContentsLayer(solid_color_layer_.get());
    solid_color_layer_->RemoveFromParent();
    solid_color_layer_ = nullptr;
  }
  if (!surface_layer_) {
    // This covers non-video cases, where we don't create the SurfaceLayer
    // early.
    // TODO(lethalantidote): Eliminate this case. Once you do that, you can
    // also just store the surface_id and not the frame_sink_id.
    CreateSurfaceLayer();
  }

  current_surface_id_ = surface_info.id();

  surface_layer_->SetSurfaceId(surface_info.id(),
                               cc::DeadlinePolicy::UseSpecifiedDeadline(0u));
  surface_layer_->SetOldestAcceptableFallback(surface_info.id());

  if (observer_) {
    observer_->OnWebLayerUpdated();
    observer_->OnSurfaceIdUpdated(surface_info.id());
  }

  surface_layer_->SetContentsOpaque(opaque_);
}

cc::Layer* SurfaceLayerBridge::GetCcLayer() const {
  if (surface_layer_)
    return surface_layer_.get();

  return solid_color_layer_.get();
}

const viz::FrameSinkId& SurfaceLayerBridge::GetFrameSinkId() const {
  return frame_sink_id_;
}

void SurfaceLayerBridge::ClearSurfaceId() {
  current_surface_id_ = viz::SurfaceId();

  if (!surface_layer_)
    return;

  // We reset the Ids if we lose the context_provider (case: GPU process ended)
  // If we destroyed the surface_layer before that point, we need not update
  // the ids.
  surface_layer_->SetSurfaceId(viz::SurfaceId(),
                               cc::DeadlinePolicy::UseDefaultDeadline());
  surface_layer_->SetOldestAcceptableFallback(viz::SurfaceId());
}

void SurfaceLayerBridge::SetContentsOpaque(bool opaque) {
  // If the surface isn't activated, we have nothing to show, do not change
  // opacity (defaults to false on surface_layer creation).
  if (surface_layer_ && surface_activated_)
    surface_layer_->SetContentsOpaque(opaque);
  opaque_ = opaque;
}

void SurfaceLayerBridge::CreateSurfaceLayer() {
  surface_layer_ = cc::SurfaceLayer::Create(update_submission_state_callback_);

  // This surface_id is essentially just a placeholder for the real one we will
  // get in OnFirstSurfaceActivation. We need it so that we properly get a
  // WillDraw, which then pushes the first compositor frame.
  current_surface_id_ = viz::SurfaceId(
      frame_sink_id_,
      parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId());

  surface_layer_->SetSurfaceId(current_surface_id_,
                               cc::DeadlinePolicy::UseDefaultDeadline());

  surface_layer_->SetStretchContentToFillBounds(true);
  surface_layer_->SetIsDrawable(true);
  surface_layer_->SetMayContainVideo(true);

  if (observer_) {
    observer_->RegisterContentsLayer(surface_layer_.get());
  }
  // We ignore our opacity until we are sure that we have something to show,
  // as indicated by getting an OnFirstSurfaceActivation call.
  surface_layer_->SetContentsOpaque(false);
}

base::TimeTicks SurfaceLayerBridge::GetLocalSurfaceIdAllocationTime() const {
  return parent_local_surface_id_allocator_.allocation_time();
}

}  // namespace blink
