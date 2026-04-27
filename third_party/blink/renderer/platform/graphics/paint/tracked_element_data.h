// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRACKED_ELEMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRACKED_ELEMENT_DATA_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/token.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/tracked_element_id.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
enum class TrackedElementFeature;
}  // namespace viz

namespace blink {

// Represents a single tracked element, either tracking a full element
// rectangle, or an arbitrary rectangle relative to the element.
struct PLATFORM_EXPORT TrackedElementSubRect {
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

  TrackedElementSubRect() = default;
  explicit TrackedElementSubRect(
      TrackedElementId id,
      bool should_add_to_compositor_frame_metadata = false,
      std::optional<SubRect> sub_rect = std::nullopt,
      std::optional<FrameToken> frame_token = std::nullopt,
      std::optional<LocalFrameToken> parent_frame_token = std::nullopt)
      : id(id),
        should_add_to_compositor_frame_metadata(
            should_add_to_compositor_frame_metadata),
        sub_rect(sub_rect),
        frame_token(frame_token),
        parent_frame_token(parent_frame_token) {}

  TrackedElementId id;
  // Whether the element should be added to the compositor frame metadata. If
  // false, the element will be added to the render frame metadata.
  bool should_add_to_compositor_frame_metadata;
  // The sub-rectangle of the element to track, or nullopt if the entire
  // element is being tracked.
  std::optional<SubRect> sub_rect;
  // The frame token of the frame containing the element being tracked.
  std::optional<FrameToken> frame_token;
  // The local frame token of the parent frame.
  std::optional<LocalFrameToken> parent_frame_token;

  // Comparison operators for use with WTF::HashSet and other containers.
  bool operator==(const TrackedElementSubRect& other) const {
    return id == other.id &&
           should_add_to_compositor_frame_metadata ==
               other.should_add_to_compositor_frame_metadata &&
           sub_rect == other.sub_rect && frame_token == other.frame_token &&
           parent_frame_token == other.parent_frame_token;
  }
  bool operator!=(const TrackedElementSubRect& other) const {
    return !(*this == other);
  }

  gfx::Rect GetEffectiveBounds(const gfx::Rect& element_paint_rect) const;
};

// Used by Element and ElementRareDataVector to store tracked element data.
// Multiple features can track the same element, so this is a map of feature to
// the tracked element data for that feature.
using TrackedElementSubRects =
    base::flat_map<viz::TrackedElementFeature, TrackedElementSubRect>;

// Represents the data associated with a tracked element. This includes the
// id of the element, the bounds of the element in screen space, and other
// optional metadata that may be set by the tracking feature.
struct PLATFORM_EXPORT TrackedElementRect {
  TrackedElementRect() = default;
  TrackedElementRect(
      TrackedElementId id,
      gfx::Rect bounds,
      bool should_add_to_compositor_frame_metadata = false,
      std::optional<FrameToken> frame_token = std::nullopt,
      std::optional<LocalFrameToken> parent_frame_token = std::nullopt)
      : id(id),
        bounds(bounds),
        should_add_to_compositor_frame_metadata(
            should_add_to_compositor_frame_metadata),
        frame_token(frame_token),
        parent_frame_token(parent_frame_token) {}

  // The id of the element being tracked.
  TrackedElementId id;
  // The bounds of the element in screen space.
  gfx::Rect bounds;
  // Whether the element should be added to the compositor frame metadata. If
  // false, the element will be added to the render frame metadata.
  bool should_add_to_compositor_frame_metadata;
  // The frame token of the frame containing the element being tracked.
  std::optional<FrameToken> frame_token;
  // The local frame token of the parent frame.
  std::optional<LocalFrameToken> parent_frame_token;

  // Comparison operators for use with WTF::HashSet and other containers.
  bool operator==(const TrackedElementRect& other) const = default;

  String ToString() const;
};

// Wraps a map from a tracked element feature to the list of tracked elements
// being tracked by that feature. The element rectangle represents the bounds of
// the HTML element in screen space.
struct PLATFORM_EXPORT TrackedElementRects
    : public GarbageCollected<TrackedElementRects> {
  base::flat_map<viz::TrackedElementFeature, std::vector<TrackedElementRect>>
      map;

  bool operator==(const TrackedElementRects& rhs) const {
    return map == rhs.map;
  }

  void Trace(Visitor*) const {}

  String ToString() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRACKED_ELEMENT_DATA_H_
