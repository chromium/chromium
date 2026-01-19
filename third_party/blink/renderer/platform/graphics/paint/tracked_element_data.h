// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRACKED_ELEMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRACKED_ELEMENT_DATA_H_

#include "base/containers/flat_map.h"
#include "base/token.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/tracked_element_id.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

// Represents a single tracked element, either tracking a full element
// rectangle, or an arbitrary rectangle relative to the element.
struct PLATFORM_EXPORT TrackedElementRect {
  struct SubRect {
    enum class Type {
      // While tracking the rectangle, the output is intersection of specified
      // sub-rectangle and the element box rectangle.
      kIntersectWithElementRect,
      // While tracking the rectangle, the output is just the rectangle relative
      // to the element. It may be actually located, or be larger then element
      // box.
      kNoIntersection,
    };

    // A subset relative to this element's visual/painted rect. The visual rect
    // includes all effects from the element like shadow, filter etc.
    gfx::Rect rect;
    Type type = Type::kIntersectWithElementRect;

    bool operator==(const SubRect& other) const = default;
  };

  TrackedElementRect() = default;
  explicit TrackedElementRect(TrackedElementId id,
                              std::optional<SubRect> sub_rect = std::nullopt)
      : id(id), sub_rect(sub_rect) {}

  TrackedElementId id;
  std::optional<SubRect> sub_rect;

  // Comparison operators for use with WTF::HashSet and other containers.
  bool operator==(const TrackedElementRect& other) const {
    return id == other.id && sub_rect == other.sub_rect;
  }
  bool operator!=(const TrackedElementRect& other) const {
    return !(*this == other);
  }

  gfx::Rect GetEffectiveBounds(const gfx::Rect& element_paint_rect) const;
};

// Wraps a map from a tracked element identifier, which is a randomly
// generated token, to a rectangle representing the bounds of the HTML element
// associated with the crop identifier.
struct PLATFORM_EXPORT TrackedElementData
    : public GarbageCollected<TrackedElementData> {
  base::flat_map<TrackedElementId, gfx::Rect> map;

  bool operator==(const TrackedElementData& rhs) const {
    return map == rhs.map;
  }

  void Trace(Visitor*) const {}

  String ToString() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRACKED_ELEMENT_DATA_H_
