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
  explicit RefCountedPropertyTreeState(
      const PropertyTreeStateOrAlias& property_tree_state)
      : transform_(&property_tree_state.Transform()),
        clip_(&property_tree_state.Clip()),
        effect_(&property_tree_state.Effect()) {}

  RefCountedPropertyTreeState& operator=(
      const PropertyTreeStateOrAlias& property_tree_state) {
    return *this = RefCountedPropertyTreeState(property_tree_state);
  }

  const TransformPaintPropertyNodeOrAlias& Transform() const {
    return *transform_;
  }
  const ClipPaintPropertyNodeOrAlias& Clip() const { return *clip_; }
  const EffectPaintPropertyNodeOrAlias& Effect() const { return *effect_; }

  PropertyTreeStateOrAlias GetPropertyTreeState() const {
    return PropertyTreeStateOrAlias(Transform(), Clip(), Effect());
  }

  void ClearChangedToRoot() const {
    Transform().ClearChangedToRoot();
    Clip().ClearChangedToRoot();
    Effect().ClearChangedToRoot();
  }

  String ToString() const { return GetPropertyTreeState().ToString(); }
#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const { return GetPropertyTreeState().ToTreeString(); }
#endif

 private:
  scoped_refptr<const TransformPaintPropertyNodeOrAlias> transform_;
  scoped_refptr<const ClipPaintPropertyNodeOrAlias> clip_;
  scoped_refptr<const EffectPaintPropertyNodeOrAlias> effect_;
};

inline bool operator==(const RefCountedPropertyTreeState& a,
                       const RefCountedPropertyTreeState& b) {
  return &a.Transform() == &b.Transform() && &a.Clip() == &b.Clip() &&
         &a.Effect() == &b.Effect();
}

inline bool operator!=(const RefCountedPropertyTreeState& a,
                       const RefCountedPropertyTreeState& b) {
  return !(a == b);
}

inline bool operator==(const RefCountedPropertyTreeState& a,
                       const PropertyTreeStateOrAlias& b) {
  return &a.Transform() == &b.Transform() && &a.Clip() == &b.Clip() &&
         &a.Effect() == &b.Effect();
}

inline bool operator!=(const RefCountedPropertyTreeState& a,
                       const PropertyTreeStateOrAlias& b) {
  return !(a == b);
}

inline bool operator==(const PropertyTreeStateOrAlias& a,
                       const RefCountedPropertyTreeState& b) {
  return b == a;
}

inline bool operator!=(const PropertyTreeStateOrAlias& a,
                       const RefCountedPropertyTreeState& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_
