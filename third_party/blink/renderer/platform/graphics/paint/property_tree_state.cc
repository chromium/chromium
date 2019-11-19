// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

#include <memory>

namespace blink {

const PropertyTreeState& PropertyTreeState::Uninitialized() {
  DEFINE_STATIC_REF(const TransformPaintPropertyNode, transform,
                    TransformPaintPropertyNode::Create(
                        TransformPaintPropertyNode::Root(), {}));
  DEFINE_STATIC_REF(const ClipPaintPropertyNode, clip,
                    ClipPaintPropertyNode::Create(ClipPaintPropertyNode::Root(),
                                                  {transform}));
  DEFINE_STATIC_REF(const EffectPaintPropertyNode, effect,
                    EffectPaintPropertyNode::Create(
                        EffectPaintPropertyNode::Root(), {transform}));
  DEFINE_STATIC_LOCAL(const PropertyTreeState, uninitialized,
                      (*transform, *clip, *effect));
  return uninitialized;
}

const PropertyTreeState& PropertyTreeState::Root() {
  DEFINE_STATIC_LOCAL(
      const PropertyTreeState, root,
      (TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
       EffectPaintPropertyNode::Root()));
  return root;
}

PropertyTreeState PropertyTreeState::Unalias() const {
  return PropertyTreeState(Transform().Unalias(), Clip().Unalias(),
                           Effect().Unalias());
}

String PropertyTreeState::ToString() const {
  return String::Format("t:%p c:%p e:%p", transform_, clip_, effect_);
}

#if DCHECK_IS_ON()

String PropertyTreeState::ToTreeString() const {
  return "transform:\n" + Transform().ToTreeString() + "\nclip:\n" +
         Clip().ToTreeString() + "\neffect:\n" + Effect().ToTreeString();
}

#endif

size_t PropertyTreeState::CacheMemoryUsageInBytes() const {
  return Clip().CacheMemoryUsageInBytes() +
         Transform().CacheMemoryUsageInBytes();
}

std::ostream& operator<<(std::ostream& os, const PropertyTreeState& state) {
  return os << state.ToString().Utf8();
}

}  // namespace blink
