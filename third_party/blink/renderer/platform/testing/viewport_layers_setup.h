// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIEWPORT_LAYERS_SETUP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIEWPORT_LAYERS_SETUP_H_

#include <memory>

#include "third_party/blink/renderer/platform/testing/fake_graphics_layer_client.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace cc {
class AnimationHost;
class LayerTreeHost;
}  // namespace cc

namespace blink {

class FakeGraphicsLayer;
class LayerTreeHostEmbedder;

class ViewportLayersSetup {
  DISALLOW_NEW();

 public:
  ViewportLayersSetup();
  ~ViewportLayersSetup();

  FakeGraphicsLayer& graphics_layer() { return *graphics_layer_; }
  FakeGraphicsLayerClient& graphics_layer_client() { return client_; }

  cc::LayerTreeHost* layer_tree_host();
  cc::AnimationHost* animation_host();

 private:
  std::unique_ptr<FakeGraphicsLayer> root_layer_;
  std::unique_ptr<FakeGraphicsLayer> graphics_layer_;
  FakeGraphicsLayerClient client_;
  std::unique_ptr<LayerTreeHostEmbedder> layer_tree_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIEWPORT_LAYERS_SETUP_H_
