// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/viewport_layers_setup.h"

#include <memory>
#include "base/time/time.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer_client.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"

namespace blink {

ViewportLayersSetup::ViewportLayersSetup() {
  clip_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
  scroll_elasticity_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
  page_scale_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
  graphics_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
  graphics_layer_->SetDrawsContent(true);
  clip_layer_->AddChild(scroll_elasticity_layer_.get());
  scroll_elasticity_layer_->AddChild(page_scale_layer_.get());
  page_scale_layer_->AddChild(graphics_layer_.get());
  graphics_layer_->CcLayer()->SetScrollable(clip_layer_->CcLayer()->bounds());
  layer_tree_ = std::make_unique<LayerTreeHostEmbedder>();
  layer_tree_->layer_tree_host()->SetRootLayer(clip_layer_->CcLayer());

  scroll_elasticity_layer_->SetElementId(cc::LayerIdToElementIdForTesting(
      scroll_elasticity_layer_->CcLayer()->id()));

  cc::LayerTreeHost::ViewportLayers viewport_layers;
  viewport_layers.overscroll_elasticity_element_id =
      scroll_elasticity_layer_->GetElementId();
  viewport_layers.page_scale = page_scale_layer_->CcLayer();
  viewport_layers.inner_viewport_container = clip_layer_->CcLayer();
  viewport_layers.inner_viewport_scroll = graphics_layer_->CcLayer();
  layer_tree_->layer_tree_host()->RegisterViewportLayers(viewport_layers);
  layer_tree_->layer_tree_host()->SetViewportSizeAndScale(
      gfx::Size(1, 1), /*device_scale_factor=*/1.f, viz::LocalSurfaceId(),
      base::TimeTicks());

  graphics_layer_->SetLayerState(PropertyTreeState(PropertyTreeState::Root()),
                                 IntPoint());
}

ViewportLayersSetup::~ViewportLayersSetup() = default;

cc::LayerTreeHost* ViewportLayersSetup::layer_tree_host() {
  return layer_tree_->layer_tree_host();
}

cc::AnimationHost* ViewportLayersSetup::animation_host() {
  return layer_tree_->animation_host();
}

}  // namespace blink
