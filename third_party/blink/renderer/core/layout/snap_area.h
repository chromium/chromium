// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SNAP_AREA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SNAP_AREA_H_

#include <optional>

#include "base/check.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

// Represents a scroll snap area during layout fragment propagation.
//
// Captures the `WritingDirectionMode` (`writing_direction_mode_`) of the
// nearest ancestor scroll container and uses it to resolve logical alignments
// in `scroll-snap-align` into physical axes.
class SnapArea {
  DISALLOW_NEW();

 public:
  SnapArea() = default;
  explicit SnapArea(Element* element) : element_(element) {}

  SnapArea(Element* element,
           PhysicalAxes consumed,
           PhysicalAxes pending,
           std::optional<WritingDirectionMode> writing_direction_mode)
      : element_(element),
        consumed_axes_(consumed),
        pending_axes_(pending),
        writing_direction_mode_(writing_direction_mode) {}

  Element* GetElementIfConsumed() const {
    CHECK(Resolved());
    return consumed_axes_ != kPhysicalAxesNone ? element_.Get() : nullptr;
  }

  bool IsPending() const {
    return !Resolved() || pending_axes_ != kPhysicalAxesNone;
  }

  Element* GetElement() const { return element_.Get(); }

  PhysicalAxes ConsumedAxes() const {
    CHECK(Resolved());
    return consumed_axes_;
  }
  PhysicalAxes PendingAxes() const {
    CHECK(Resolved());
    return pending_axes_;
  }

  bool Resolved() const { return writing_direction_mode_.has_value(); }

  std::optional<WritingDirectionMode> ContainerWritingDirectionMode() const {
    return writing_direction_mode_;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(element_); }

  bool operator==(const SnapArea&) const = default;

 private:
  Member<Element> element_;
  PhysicalAxes consumed_axes_ = kPhysicalAxesNone;
  PhysicalAxes pending_axes_ = kPhysicalAxesNone;
  std::optional<WritingDirectionMode> writing_direction_mode_;
};

template <>
struct VectorTraits<SnapArea> : VectorTraitsBase<SnapArea> {
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SNAP_AREA_H_
