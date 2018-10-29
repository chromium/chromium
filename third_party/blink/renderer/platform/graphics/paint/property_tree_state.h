// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PROPERTY_TREE_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PROPERTY_TREE_STATE_H_

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.
class PLATFORM_EXPORT PropertyTreeState {
  USING_FAST_MALLOC(PropertyTreeState);

 public:
  PropertyTreeState(const TransformPaintPropertyNode* transform,
                    const ClipPaintPropertyNode* clip,
                    const EffectPaintPropertyNode* effect)
      : transform_(transform), clip_(clip), effect_(effect) {}

  // Returns an unaliased property tree state.
  PropertyTreeState Unalias() const;

  const TransformPaintPropertyNode* Transform() const { return transform_; }
  void SetTransform(const TransformPaintPropertyNode* node) {
    transform_ = node;
  }

  const ClipPaintPropertyNode* Clip() const { return clip_; }
  void SetClip(const ClipPaintPropertyNode* node) { clip_ = node; }

  const EffectPaintPropertyNode* Effect() const { return effect_; }
  void SetEffect(const EffectPaintPropertyNode* node) { effect_ = node; }

  static const PropertyTreeState& Root();

  // Returns the compositor element id, if any, for this property state. If
  // neither the effect nor transform nodes have a compositor element id then a
  // default instance is returned.
  const CompositorElementId GetCompositorElementId(
      const CompositorElementIdSet& element_ids) const;

  void ClearChangedToRoot() const {
    Transform()->ClearChangedToRoot();
    Clip()->ClearChangedToRoot();
    Effect()->ClearChangedToRoot();
  }

  String ToString() const;
#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const;
#endif

  // Returns memory usage of the transform & clip caches of this state plus
  // ancestors.
  size_t CacheMemoryUsageInBytes() const;

 private:
  const TransformPaintPropertyNode* transform_;
  const ClipPaintPropertyNode* clip_;
  const EffectPaintPropertyNode* effect_;
};

inline bool operator==(const PropertyTreeState& a, const PropertyTreeState& b) {
  return a.Transform() == b.Transform() && a.Clip() == b.Clip() &&
         a.Effect() == b.Effect();
}

inline bool operator!=(const PropertyTreeState& a, const PropertyTreeState& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PROPERTY_TREE_STATE_H_
