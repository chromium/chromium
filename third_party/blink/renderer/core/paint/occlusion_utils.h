// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OCCLUSION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OCCLUSION_UTILS_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client_types.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace gfx {
class Rect;
}

namespace blink {

class Element;
class Node;
class PaintArtifact;

// Utilities to tell how much of an element is hidden by the nodes in front of
// it. Such nodes are typically found by a penetrating list-based hit test over
// the element.

// The ids of the display item clients (e.g., LayoutObjects) that painted some
// content.
using PaintedClientIds = HashSet<DisplayItemClientId>;

// Returns the index of the first display item of `client_id` in
// `paint_artifact`, if any. Display items are stored in paint order, from back
// to front, so those after it are painted on top of it.
CORE_EXPORT std::optional<wtf_size_t> FindFirstDisplayItem(
    const PaintArtifact& paint_artifact,
    DisplayItemClientId client_id);

// Returns the ids of the clients that painted the display items of
// `paint_artifact` from `begin_index` (inclusive) to `end_index` (exclusive).
CORE_EXPORT PaintedClientIds
CollectPaintedClientIds(const PaintArtifact& paint_artifact,
                        wtf_size_t begin_index,
                        wtf_size_t end_index);

// How a hit node affects the visibility of a target element.
enum class HitNodeOcclusion {
  // The node is the target itself.
  kTarget,
  // The node hides part of the target.
  kOccluding,
  // The node is part of its host's rendering (e.g., the controls of a video).
  // It's up to the caller whether it hides the target.
  kInUserAgentShadowTree,
  // The node is invisible due to a zero opacity on it or an ancestor.
  kZeroOpacity,
  // The node didn't paint anything that hides what's behind it.
  kNotPainted,
};

// Tracks how much of a target element is hidden by the nodes in front of it,
// as a penetrating list-based hit test over the target hits them, from front
// to back (see HitTestRequest::HitNodeCb). It's up to the caller to tell the
// bounds of an occluding node, and to decide how much of the target must
// remain visible, hence when to stop the hit test.
class CORE_EXPORT HitTestOcclusionTracker {
  DISALLOW_NEW();

 public:
  // Only the clients in `occluding_client_ids` hide anything (see
  // CollectPaintedClientIds()), or any node if it's not set.
  explicit HitTestOcclusionTracker(
      const Element& target,
      std::optional<PaintedClientIds> occluding_client_ids = std::nullopt);

  // Returns how `node`, a node hit by the hit test, affects the visibility of
  // the target.
  HitNodeOcclusion GetHitNodeOcclusion(const Node& node) const;

  // Records that `rect` is hidden (e.g., by an occluding node, or by the edge
  // of the viewport).
  void AddOccludedRect(const gfx::Rect& rect);

  // Returns the total area recorded as hidden so far.
  float OccludedArea() const;

  // Forgets the area recorded so far, to track another hit test.
  void Reset();

  void Trace(Visitor* visitor) const;

 private:
  Member<const Element> target_;
  std::optional<PaintedClientIds> occluding_client_ids_;
  SkRegion occluded_region_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OCCLUSION_UTILS_H_
