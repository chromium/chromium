// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYERED_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYERED_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CascadeLayer;

// Stores an object and an associated CascadeLayer.
//
// For example, if you specify the following:
//
//  @layer foo {
//    @keyframes anim { /* ... */ }
//  }
//
// Then, in RuleSet::keyframes_rules_, there will be an entry of this
// class with T=StyleRuleKeyframes and with the 'layer' member set
// to the CascadeLayer representing "foo".
//
// https://drafts.csswg.org/css-cascade-5/#layering
template <typename T>
class CascadeLayered {
  DISALLOW_NEW();

 public:
  CascadeLayered() = default;

  // Needed to allow CascadeLayered<T>(static_cast<T*>(...), layer).
  template <typename U>
    requires std::convertible_to<U, AddMemberIfNeeded<T>>
  CascadeLayered(U value, const CascadeLayer* layer)
      : value(std::move(value)), layer(layer) {}

  // Needed to convert CascadeLayered<T> to CascadeLayered<const T>.
  template <typename U>
    requires std::convertible_to<U, T>
  CascadeLayered(const CascadeLayered<U>& other)  // NOLINT
      : value(other.value), layer(other.layer) {}

  void Trace(blink::Visitor* visitor) const {
    TraceIfNeeded<AddMemberIfNeeded<T>>::Trace(visitor, value);
    visitor->Trace(layer);
  }

  AddMemberIfNeeded<T> value;
  Member<const CascadeLayer> layer;
};

template <typename T>
struct VectorTraits<CascadeLayered<T>> : VectorTraitsBase<CascadeLayered<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kCanInitializeWithMemset =
      VectorTraits<AddMemberIfNeeded<T>>::kCanInitializeWithMemset &&
      VectorTraits<Member<const CascadeLayer>>::kCanInitializeWithMemset;
  static const bool kCanClearUnusedSlotsWithMemset =
      VectorTraits<AddMemberIfNeeded<T>>::kCanClearUnusedSlotsWithMemset &&
      VectorTraits<Member<const CascadeLayer>>::kCanClearUnusedSlotsWithMemset;
  static const bool kCanMoveWithMemcpy =
      VectorTraits<AddMemberIfNeeded<T>>::kCanMoveWithMemcpy &&
      VectorTraits<Member<const CascadeLayer>>::kCanMoveWithMemcpy;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CASCADE_LAYERED_H_
