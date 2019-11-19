// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_client.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_target_property.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer_client.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/viewport_layers_setup.h"

namespace blink {

class AnimatedLayersTest : public testing::Test,
                           public PaintTestConfigurations {
 public:
  AnimatedLayersTest() = default;
  ~AnimatedLayersTest() = default;

 protected:
  ViewportLayersSetup layers_;
};

class AnimationForTesting : public CompositorAnimationClient {
 public:
  AnimationForTesting() {
    compositor_animation_ = CompositorAnimation::Create();
  }

  CompositorAnimation* GetCompositorAnimation() const override {
    return compositor_animation_.get();
  }

  std::unique_ptr<CompositorAnimation> compositor_animation_;
};

}  // namespace blink
