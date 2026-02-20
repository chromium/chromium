// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SPLIT_AXIS_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SPLIT_AXIS_ITEM_H_

#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

// Tracks a layout object during metadata propagation (e.g. sticky positioning
// or scroll snap areas).
//
// As the object bubbles up the fragment tree, its constraints can be split
// across physical axes. Each fragment partitions the axes into:
//   - Consumed: Axes resolved locally by the current fragment.
//   - Pending:  Axes that remain unresolved and must continue bubbling.
template <class T>
class SplitAxisItem {
  DISALLOW_NEW();

 public:
  SplitAxisItem() = default;
  SplitAxisItem(T* value, PhysicalAxes consumed_axes, PhysicalAxes pending_axes)
      : value_(value),
        consumed_axes_(consumed_axes),
        pending_axes_(pending_axes) {}

  // Returns the object if the current fragment consumed it on at least one
  // axis.
  T* GetIfConsumed() const {
    return consumed_axes_ != kPhysicalAxesNone ? value_.Get() : nullptr;
  }

  // Returns the object if it still needs to propagate on at least one axis.
  T* GetIfPending() const {
    return pending_axes_ != kPhysicalAxesNone ? value_.Get() : nullptr;
  }

  PhysicalAxes ConsumedAxes() const { return consumed_axes_; }
  PhysicalAxes PendingAxes() const { return pending_axes_; }

  void Trace(Visitor* visitor) const { visitor->Trace(value_); }
  bool operator==(const SplitAxisItem&) const = default;

 private:
  Member<T> value_;
  PhysicalAxes consumed_axes_ = kPhysicalAxesNone;
  PhysicalAxes pending_axes_ = kPhysicalAxesNone;
};

template <typename T>
struct VectorTraits<SplitAxisItem<T>> : VectorTraitsBase<SplitAxisItem<T>> {
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SPLIT_AXIS_ITEM_H_
