// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/occlusion_utils.h"

#include <utility>

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

std::optional<wtf_size_t> FindFirstDisplayItem(
    const PaintArtifact& paint_artifact,
    DisplayItemClientId client_id) {
  const DisplayItemList& display_item_list =
      paint_artifact.GetDisplayItemList();
  for (wtf_size_t index = 0; index < display_item_list.size(); ++index) {
    if (display_item_list[index].ClientId() == client_id) {
      return index;
    }
  }
  return std::nullopt;
}

PaintedClientIds CollectPaintedClientIds(const PaintArtifact& paint_artifact,
                                         wtf_size_t begin_index,
                                         wtf_size_t end_index) {
  const DisplayItemList& display_item_list =
      paint_artifact.GetDisplayItemList();
  PaintedClientIds client_ids;
  for (const DisplayItem& item :
       display_item_list.ItemsInRange(begin_index, end_index)) {
    if (item.ClientId() != kInvalidDisplayItemClientId) {
      client_ids.insert(item.ClientId());
    }
  }
  return client_ids;
}

HitTestOcclusionTracker::HitTestOcclusionTracker(
    const Element& target,
    std::optional<PaintedClientIds> occluding_client_ids)
    : target_(&target),
      occluding_client_ids_(std::move(occluding_client_ids)) {}

HitNodeOcclusion HitTestOcclusionTracker::GetHitNodeOcclusion(
    const Node& node) const {
  if (&node == target_) {
    return HitNodeOcclusion::kTarget;
  }
  if (node.IsInUserAgentShadowRoot()) {
    return HitNodeOcclusion::kInUserAgentShadowTree;
  }
  const LayoutObject* object = node.GetLayoutObject();
  if (!object) {
    return HitNodeOcclusion::kNotPainted;
  }
  if (!object->HasNonZeroEffectiveOpacity()) {
    return HitNodeOcclusion::kZeroOpacity;
  }
  if (occluding_client_ids_ && !occluding_client_ids_->Contains(object->Id())) {
    return HitNodeOcclusion::kNotPainted;
  }
  return HitNodeOcclusion::kOccluding;
}

void HitTestOcclusionTracker::AddOccludedRect(const gfx::Rect& rect) {
  occluded_region_.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
}

float HitTestOcclusionTracker::OccludedArea() const {
  float area = 0.0f;
  for (SkRegion::Iterator it(occluded_region_); !it.done(); it.next()) {
    area += static_cast<float>(it.rect().width()) *
            static_cast<float>(it.rect().height());
  }
  return area;
}

void HitTestOcclusionTracker::Reset() {
  occluded_region_.setEmpty();
}

void HitTestOcclusionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
}

}  // namespace blink
