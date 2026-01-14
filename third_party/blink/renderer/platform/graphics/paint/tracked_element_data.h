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

// Represents a single tracked element, currently it tracks full element
// rectangle, will be extended to allow tracking of subsection with a specific
// rectangle.
struct PLATFORM_EXPORT TrackedElementRect {
  TrackedElementRect() = default;
  explicit TrackedElementRect(TrackedElementId id) : id(id) {}

  static std::unique_ptr<TrackedElementRect> CreateFull(TrackedElementId id) {
    return std::make_unique<TrackedElementRect>(id);
  }

  TrackedElementId id;

  // Comparison operators for use with WTF::HashSet and other containers.
  bool operator==(const TrackedElementRect& other) const {
    return id == other.id;
  }
  bool operator!=(const TrackedElementRect& other) const {
    return !(*this == other);
  }
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
