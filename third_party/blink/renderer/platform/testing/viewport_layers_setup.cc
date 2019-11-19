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
  // TODO(wangxianzhu): Don't create unnecessary layers.
  root_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
  graphics_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
  graphics_layer_->SetDrawsContent(true);
  graphics_layer_->SetHitTestable(true);
  root_layer_->AddChild(graphics_layer_.get());
  graphics_layer_->CcLayer()->SetScrollable(root_layer_->CcLayer()->bounds());
  layer_tree_ = std::make_unique<LayerTreeHostEmbedder>();
  layer_tree_->layer_tree_host()->SetRootLayer(root_layer_->CcLayer());

  layer_tree_->layer_tree_host()->SetViewportRectAndScale(
      gfx::Rect(1, 1), /*device_scale_factor=*/1.f,
      viz::LocalSurfaceIdAllocation());

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
