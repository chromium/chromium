// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cascade_layer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CascadeLayerTest : public testing::Test {
 public:
  CascadeLayerTest() : root_layer_(MakeGarbageCollected<CascadeLayer>()) {}

  using LayerName = StyleRuleBase::LayerName;

 protected:
  String LayersToString() const { return root_layer_->ToStringForTesting(); }

  Persistent<CascadeLayer> root_layer_;
};

TEST_F(CascadeLayerTest, Basic) {
  CascadeLayer* one =
      root_layer_->GetOrAddSubLayer(LayerName({AtomicString("one")}));
  one->GetOrAddSubLayer(LayerName({AtomicString("two")}));
  root_layer_->GetOrAddSubLayer(
      LayerName({AtomicString("three"), AtomicString("four")}));
  root_layer_->GetOrAddSubLayer(LayerName({g_empty_atom}));
  root_layer_->GetOrAddSubLayer(LayerName({AtomicString("five")}));

  EXPECT_EQ(
      "one,"
      "one.two,"
      "three,"
      "three.four,"
      "(anonymous),"
      "five",
      LayersToString());
}

TEST_F(CascadeLayerTest, RepeatedGetOrAdd) {
  // GetOrAddSubLayer() does not add duplicate layers.

  root_layer_->GetOrAddSubLayer(
      LayerName({AtomicString("one"), AtomicString("two")}));
  root_layer_->GetOrAddSubLayer(LayerName({AtomicString("three")}));

  root_layer_->GetOrAddSubLayer(LayerName({AtomicString("one")}))
      ->GetOrAddSubLayer(LayerName({AtomicString("two")}));
  root_layer_->GetOrAddSubLayer(LayerName({AtomicString("three")}));

  EXPECT_EQ(
      "one,"
      "one.two,"
      "three",
      LayersToString());
}

TEST_F(CascadeLayerTest, RepeatedGetOrAddAnonymous) {
  // All anonymous layers are distinct and are hence not duplicates.

  // Two distinct anonymous layers
  root_layer_->GetOrAddSubLayer(LayerName({g_empty_atom}));
  root_layer_->GetOrAddSubLayer(LayerName({g_empty_atom}));

  // Two distinct anonymous sublayers of "one"
  CascadeLayer* one =
      root_layer_->GetOrAddSubLayer(LayerName({AtomicString("one")}));
  root_layer_->GetOrAddSubLayer(LayerName({AtomicString("one"), g_empty_atom}));
  CascadeLayer* anonymous = one->GetOrAddSubLayer(LayerName({g_empty_atom}));

  anonymous->GetOrAddSubLayer(LayerName({AtomicString("two")}));

  // This is a different layer "two" from the previously inserted "two" because
  // the parent layers are different anonymous layers.
  root_layer_->GetOrAddSubLayer(
      LayerName({AtomicString("one"), g_empty_atom, AtomicString("two")}));

  EXPECT_EQ(
      "(anonymous),"
      "(anonymous),"
      "one,"
      "one.(anonymous),"
      "one.(anonymous),"
      "one.(anonymous).two,"
      "one.(anonymous),"
      "one.(anonymous).two",
      LayersToString());
}

TEST_F(CascadeLayerTest, LayerOrderNotInsertionOrder) {
  // Layer order and insertion order can be different.

  root_layer_->GetOrAddSubLayer(LayerName({AtomicString("one")}));
  root_layer_->GetOrAddSubLayer(LayerName({AtomicString("two")}));
  root_layer_->GetOrAddSubLayer(
      LayerName({AtomicString("one"), AtomicString("three")}));

  EXPECT_EQ(
      "one,"
      "one.three,"
      "two",
      LayersToString());
}

}  // namespace blink
