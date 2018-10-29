// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.  RefPtrs are used to guard against use-after-free bugs.
class PLATFORM_EXPORT RefCountedPropertyTreeState {
  USING_FAST_MALLOC(RefCountedPropertyTreeState);

 public:
  RefCountedPropertyTreeState(const TransformPaintPropertyNode* transform,
                              const ClipPaintPropertyNode* clip,
                              const EffectPaintPropertyNode* effect)
      : transform_(transform), clip_(clip), effect_(effect) {}

  RefCountedPropertyTreeState(const PropertyTreeState& property_tree_state)
      : transform_(property_tree_state.Transform()),
        clip_(property_tree_state.Clip()),
        effect_(property_tree_state.Effect()) {}

  bool HasDirectCompositingReasons() const;

  const TransformPaintPropertyNode* Transform() const {
    return transform_.get();
  }
  void SetTransform(scoped_refptr<const TransformPaintPropertyNode> node) {
    transform_ = std::move(node);
  }

  const ClipPaintPropertyNode* Clip() const { return clip_.get(); }
  void SetClip(scoped_refptr<const ClipPaintPropertyNode> node) {
    clip_ = std::move(node);
  }

  const EffectPaintPropertyNode* Effect() const { return effect_.get(); }
  void SetEffect(scoped_refptr<const EffectPaintPropertyNode> node) {
    effect_ = std::move(node);
  }

  static const RefCountedPropertyTreeState& Root();

  PropertyTreeState GetPropertyTreeState() const {
    return PropertyTreeState(transform_.get(), clip_.get(), effect_.get());
  }

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

  String ToString() const { return GetPropertyTreeState().ToString(); }
#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const { return GetPropertyTreeState().ToTreeString(); }
#endif

 private:
  scoped_refptr<const TransformPaintPropertyNode> transform_;
  scoped_refptr<const ClipPaintPropertyNode> clip_;
  scoped_refptr<const EffectPaintPropertyNode> effect_;
};

inline bool operator==(const RefCountedPropertyTreeState& a,
                       const RefCountedPropertyTreeState& b) {
  return a.Transform() == b.Transform() && a.Clip() == b.Clip() &&
         a.Effect() == b.Effect();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_
