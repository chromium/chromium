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
  PropertyTreeState(const TransformPaintPropertyNode& transform,
                    const ClipPaintPropertyNode& clip,
                    const EffectPaintPropertyNode& effect)
      : transform_(&transform), clip_(&clip), effect_(&effect) {
    DCHECK(transform_);
    DCHECK(clip_);
    DCHECK(effect_);
  }

  static const PropertyTreeState& Root();

  // This is used as the initial value of uninitialized PropertyTreeState.
  // Access to the nodes are not allowed.
  static const PropertyTreeState& Uninitialized();

  // Returns true if all fields are initialized.
  bool IsInitialized() const {
    return transform_ != Uninitialized().transform_ &&
           clip_ != Uninitialized().clip_ && effect_ != Uninitialized().effect_;
  }

  // Returns an unaliased property tree state.
  PropertyTreeState Unalias() const;

  const TransformPaintPropertyNode& Transform() const {
    DCHECK_NE(transform_, Uninitialized().transform_);
    return *transform_;
  }
  void SetTransform(const TransformPaintPropertyNode& node) {
    transform_ = &node;
    DCHECK(transform_);
  }

  const ClipPaintPropertyNode& Clip() const {
    DCHECK_NE(clip_, Uninitialized().clip_);
    return *clip_;
  }
  void SetClip(const ClipPaintPropertyNode& node) {
    clip_ = &node;
    DCHECK(clip_);
  }

  const EffectPaintPropertyNode& Effect() const {
    DCHECK_NE(effect_, Uninitialized().effect_);
    return *effect_;
  }
  void SetEffect(const EffectPaintPropertyNode& node) {
    effect_ = &node;
    DCHECK(effect_);
  }

  void ClearChangedToRoot() const {
    Transform().ClearChangedToRoot();
    Clip().ClearChangedToRoot();
    Effect().ClearChangedToRoot();
  }

  String ToString() const;
#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const;
#endif

  // Returns memory usage of the transform & clip caches of this state plus
  // ancestors.
  size_t CacheMemoryUsageInBytes() const;

  bool operator==(const PropertyTreeState& other) const {
    return transform_ == other.transform_ && clip_ == other.clip_ &&
           effect_ == other.effect_;
  }
  bool operator!=(const PropertyTreeState& other) const {
    return !(*this == other);
  }

 private:
  // For Uninitialized().
  PropertyTreeState();

  const TransformPaintPropertyNode* transform_;
  const ClipPaintPropertyNode* clip_;
  const EffectPaintPropertyNode* effect_;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const PropertyTreeState&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PROPERTY_TREE_STATE_H_
