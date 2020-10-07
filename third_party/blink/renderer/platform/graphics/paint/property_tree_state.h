// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PROPERTY_TREE_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PROPERTY_TREE_STATE_H_

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

class PropertyTreeState;

// A complete set of paint properties including those that are inherited from
// other objects.
class PLATFORM_EXPORT PropertyTreeStateOrAlias {
  USING_FAST_MALLOC(PropertyTreeStateOrAlias);

 public:
  PropertyTreeStateOrAlias(const TransformPaintPropertyNodeOrAlias& transform,
                           const ClipPaintPropertyNodeOrAlias& clip,
                           const EffectPaintPropertyNodeOrAlias& effect)
      : transform_(&transform), clip_(&clip), effect_(&effect) {
    DCHECK(transform_);
    DCHECK(clip_);
    DCHECK(effect_);
  }

  // This is used as the initial value of uninitialized PropertyTreeState.
  // Access to the nodes are not allowed.
  static PropertyTreeStateOrAlias Uninitialized() {
    return PropertyTreeStateOrAlias();
  }

  // Returns true if all fields are initialized.
  bool IsInitialized() const { return transform_ && clip_ && effect_; }

  // Returns an unaliased property tree state.
  PropertyTreeState Unalias() const;

  const TransformPaintPropertyNodeOrAlias& Transform() const {
    DCHECK(transform_);
    return *transform_;
  }
  void SetTransform(const TransformPaintPropertyNodeOrAlias& node) {
    transform_ = &node;
    DCHECK(transform_);
  }

  const ClipPaintPropertyNodeOrAlias& Clip() const {
    DCHECK(clip_);
    return *clip_;
  }
  void SetClip(const ClipPaintPropertyNodeOrAlias& node) {
    clip_ = &node;
    DCHECK(clip_);
  }

  const EffectPaintPropertyNodeOrAlias& Effect() const {
    DCHECK(effect_);
    return *effect_;
  }
  void SetEffect(const EffectPaintPropertyNodeOrAlias& node) {
    effect_ = &node;
    DCHECK(effect_);
  }

  void ClearChangedTo(const PropertyTreeStateOrAlias& to) const {
    Transform().ClearChangedTo(&to.Transform());
    Clip().ClearChangedTo(&to.Clip());
    Effect().ClearChangedTo(&to.Effect());
  }

  String ToString() const;
#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const;
#endif
  std::unique_ptr<JSONObject> ToJSON() const;

  bool operator==(const PropertyTreeStateOrAlias& other) const {
    return transform_ == other.transform_ && clip_ == other.clip_ &&
           effect_ == other.effect_;
  }
  bool operator!=(const PropertyTreeStateOrAlias& other) const {
    return !(*this == other);
  }

 protected:
  // For Uninitialized();
  PropertyTreeStateOrAlias() = default;

 private:
  const TransformPaintPropertyNodeOrAlias* transform_ = nullptr;
  const ClipPaintPropertyNodeOrAlias* clip_ = nullptr;
  const EffectPaintPropertyNodeOrAlias* effect_ = nullptr;
};

class PLATFORM_EXPORT PropertyTreeState : public PropertyTreeStateOrAlias {
 public:
  PropertyTreeState(const TransformPaintPropertyNode& transform,
                    const ClipPaintPropertyNode& clip,
                    const EffectPaintPropertyNode& effect)
      : PropertyTreeStateOrAlias(transform, clip, effect) {}

  static const PropertyTreeState& Root();

  PropertyTreeState Unalias() const = delete;

  // This is used as the initial value of uninitialized PropertyTreeState.
  // Access to the nodes are not allowed.
  static PropertyTreeState Uninitialized() { return PropertyTreeState(); }

  const TransformPaintPropertyNode& Transform() const {
    const auto& node = PropertyTreeStateOrAlias::Transform();
    DCHECK(!node.IsParentAlias());
    return static_cast<const TransformPaintPropertyNode&>(node);
  }
  void SetTransform(const TransformPaintPropertyNode& node) {
    PropertyTreeStateOrAlias::SetTransform(node);
  }
  const ClipPaintPropertyNode& Clip() const {
    const auto& node = PropertyTreeStateOrAlias::Clip();
    DCHECK(!node.IsParentAlias());
    return static_cast<const ClipPaintPropertyNode&>(node);
  }
  void SetClip(const ClipPaintPropertyNode& node) {
    PropertyTreeStateOrAlias::SetClip(node);
  }
  const EffectPaintPropertyNode& Effect() const {
    const auto& node = PropertyTreeStateOrAlias::Effect();
    DCHECK(!node.IsParentAlias());
    return static_cast<const EffectPaintPropertyNode&>(node);
  }
  void SetEffect(const EffectPaintPropertyNode& node) {
    PropertyTreeStateOrAlias::SetEffect(node);
  }

 private:
  // For Uninitialized();
  PropertyTreeState() = default;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const PropertyTreeStateOrAlias&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PROPERTY_TREE_STATE_H_
