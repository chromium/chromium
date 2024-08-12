// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.  RefPtrs are used to guard against use-after-free bugs.
class PLATFORM_EXPORT RefCountedPropertyTreeStateOrAlias {
  USING_FAST_MALLOC(RefCountedPropertyTreeStateOrAlias);

 public:
  explicit RefCountedPropertyTreeStateOrAlias(
      const PropertyTreeStateOrAlias& property_tree_state)
      : transform_(&property_tree_state.Transform()),
        clip_(&property_tree_state.Clip()),
        effect_(&property_tree_state.Effect()) {}

  RefCountedPropertyTreeStateOrAlias& operator=(
      const PropertyTreeStateOrAlias& property_tree_state) {
    return *this = RefCountedPropertyTreeStateOrAlias(property_tree_state);
  }

  const TransformPaintPropertyNodeOrAlias& Transform() const {
    return *transform_;
  }
  const ClipPaintPropertyNodeOrAlias& Clip() const { return *clip_; }
  const EffectPaintPropertyNodeOrAlias& Effect() const { return *effect_; }

  // NOTE: It would only be safe to provide setters on this class if
  // they take the property node types that are not "OrAlias", since
  // this may be an instance of RefCountedPropertyTreeState.

  PropertyTreeStateOrAlias GetPropertyTreeState() const {
    return PropertyTreeStateOrAlias(Transform(), Clip(), Effect());
  }

  void ClearChangedToRoot(int sequence_number) const {
    Transform().ClearChangedToRoot(sequence_number);
    Clip().ClearChangedToRoot(sequence_number);
    Effect().ClearChangedToRoot(sequence_number);
  }

  String ToString() const { return GetPropertyTreeState().ToString(); }
#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const { return GetPropertyTreeState().ToTreeString(); }
#endif

 protected:
  scoped_refptr<const TransformPaintPropertyNodeOrAlias> transform_;
  scoped_refptr<const ClipPaintPropertyNodeOrAlias> clip_;
  scoped_refptr<const EffectPaintPropertyNodeOrAlias> effect_;
};

class PLATFORM_EXPORT RefCountedPropertyTreeState
    : public RefCountedPropertyTreeStateOrAlias {
  USING_FAST_MALLOC(RefCountedPropertyTreeState);

 public:
  explicit RefCountedPropertyTreeState(
      const PropertyTreeState& property_tree_state)
      : RefCountedPropertyTreeStateOrAlias(property_tree_state) {}

  RefCountedPropertyTreeState& operator=(
      const PropertyTreeState& property_tree_state) {
    return *this = RefCountedPropertyTreeState(property_tree_state);
  }

  const TransformPaintPropertyNode& Transform() const {
    const auto& node = RefCountedPropertyTreeStateOrAlias::Transform();
    DCHECK(!node.IsParentAlias());
    return static_cast<const TransformPaintPropertyNode&>(node);
  }
  const ClipPaintPropertyNode& Clip() const {
    const auto& node = RefCountedPropertyTreeStateOrAlias::Clip();
    DCHECK(!node.IsParentAlias());
    return static_cast<const ClipPaintPropertyNode&>(node);
  }
  const EffectPaintPropertyNode& Effect() const {
    const auto& node = RefCountedPropertyTreeStateOrAlias::Effect();
    DCHECK(!node.IsParentAlias());
    return static_cast<const EffectPaintPropertyNode&>(node);
  }

  void SetTransform(const TransformPaintPropertyNode& transform) {
    DCHECK(&transform);
    transform_ = &transform;
  }
  void SetClip(const ClipPaintPropertyNode& clip) {
    DCHECK(&clip);
    clip_ = &clip;
  }
  void SetEffect(const EffectPaintPropertyNode& effect) {
    DCHECK(&effect);
    effect_ = &effect;
  }

  PropertyTreeState GetPropertyTreeState() const {
    return PropertyTreeState(Transform(), Clip(), Effect());
  }
};

inline bool operator==(const RefCountedPropertyTreeStateOrAlias& a,
                       const RefCountedPropertyTreeStateOrAlias& b) {
  return &a.Transform() == &b.Transform() && &a.Clip() == &b.Clip() &&
         &a.Effect() == &b.Effect();
}

inline bool operator!=(const RefCountedPropertyTreeStateOrAlias& a,
                       const RefCountedPropertyTreeStateOrAlias& b) {
  return !(a == b);
}

inline bool operator==(const RefCountedPropertyTreeStateOrAlias& a,
                       const PropertyTreeStateOrAlias& b) {
  return &a.Transform() == &b.Transform() && &a.Clip() == &b.Clip() &&
         &a.Effect() == &b.Effect();
}

inline bool operator!=(const RefCountedPropertyTreeStateOrAlias& a,
                       const PropertyTreeStateOrAlias& b) {
  return !(a == b);
}

inline bool operator==(const PropertyTreeStateOrAlias& a,
                       const RefCountedPropertyTreeStateOrAlias& b) {
  return b == a;
}

inline bool operator!=(const PropertyTreeStateOrAlias& a,
                       const RefCountedPropertyTreeStateOrAlias& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_
