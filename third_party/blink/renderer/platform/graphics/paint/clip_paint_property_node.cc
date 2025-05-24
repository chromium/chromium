// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"

#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

const gfx::RectF GetExpandedRect() {
  // Similar to InfiniteIntRect() but shifted by 4 bits to decrease floating
  // point precision errors. This rect size is still large enough to encompass
  // and reasonable paint area but not so large as to cause errors.
  constexpr int kInfiniteXY = LayoutUnit::Min().ToInt() / 64;
  constexpr int kInfiniteWH = LayoutUnit::Max().ToInt() / 32;
  return gfx::RectF(kInfiniteXY, kInfiniteXY, kInfiniteWH, kInfiniteWH);
}

}  // namespace

PaintPropertyChangeType ClipPaintPropertyNode::State::ComputeChange(
    const State& other) const {
  if (local_transform_space != other.local_transform_space ||
      paint_clip_rect_ != other.paint_clip_rect_ ||
      !ClipPathEquals(other.clip_path) ||
      pixel_moving_filter != other.pixel_moving_filter) {
    return PaintPropertyChangeType::kChangedOnlyValues;
  }
  if (layout_clip_rect_excluding_overlay_scrollbars !=
      other.layout_clip_rect_excluding_overlay_scrollbars) {
    return PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  }
  return PaintPropertyChangeType::kUnchanged;
}

void ClipPaintPropertyNode::State::Trace(Visitor* visitor) const {
  visitor->Trace(local_transform_space);
  visitor->Trace(pixel_moving_filter);
}

ClipPaintPropertyNode::ClipPaintPropertyNode(RootTag)
    : ClipPaintPropertyNodeOrAlias(kRoot),
      state_(TransformPaintPropertyNode::Root(),
             gfx::RectF(InfiniteIntRect()),
             FloatRoundedRect(InfiniteIntRect())) {}

const ClipPaintPropertyNode& ClipPaintPropertyNode::Root() {
  DEFINE_STATIC_LOCAL(Persistent<ClipPaintPropertyNode>, root,
                      (MakeGarbageCollected<ClipPaintPropertyNode>(kRoot)));
  return *root;
}

bool ClipPaintPropertyNodeOrAlias::Changed(
    PaintPropertyChangeType change,
    const PropertyTreeState& relative_to_state,
    const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const {
  for (const auto* node = this; node && node != &relative_to_state.Clip();
       node = node->Parent()) {
    if (node->NodeChanged() >= change) {
      return true;
    }
    if (node->IsParentAlias()) {
      continue;
    }
    const auto* unaliased = static_cast<const ClipPaintPropertyNode*>(node);
    if (&unaliased->LocalTransformSpace() != transform_not_to_check &&
        unaliased->LocalTransformSpace().Changed(
            change, relative_to_state.Transform())) {
      return true;
    }
  }

  return false;
}

void ClipPaintPropertyNodeOrAlias::ClearChangedToRoot(
    int sequence_number) const {
  for (auto* n = this; n && n->ChangedSequenceNumber() != sequence_number;
       n = n->Parent()) {
    n->ClearChanged(sequence_number);
    if (n->IsParentAlias())
      continue;
    static_cast<const ClipPaintPropertyNode*>(n)
        ->LocalTransformSpace()
        .ClearChangedToRoot(sequence_number);
  }
}

// static
const FloatClipRect& ClipPaintPropertyNode::ExpandedLayoutClipRect() {
  static FloatClipRect expanded_rect(GetExpandedRect());
  return expanded_rect;
}

// static
const FloatRoundedRect& ClipPaintPropertyNode::ExpandedPaintClipRect() {
  static FloatRoundedRect expanded_rect(GetExpandedRect());
  return expanded_rect;
}

std::unique_ptr<JSONObject> ClipPaintPropertyNode::ToJSON() const {
  auto json = ClipPaintPropertyNodeOrAlias::ToJSON();
  if (NodeChanged() != PaintPropertyChangeType::kUnchanged)
    json->SetString("changed", PaintPropertyChangeTypeToString(NodeChanged()));
  json->SetString("localTransformSpace",
                  String::Format("%p", state_.local_transform_space.Get()));
  json->SetString("rect", String(state_.paint_clip_rect_.Rect().ToString()));
  if (state_.layout_clip_rect_excluding_overlay_scrollbars &&
      *state_.layout_clip_rect_excluding_overlay_scrollbars !=
          state_.layout_clip_rect_) {
    json->SetString(
        "rectExcludingOverlayScrollbars",
        String(state_.layout_clip_rect_excluding_overlay_scrollbars->Rect()
                   .ToString()));
  }
  if (state_.clip_path) {
    json->SetBoolean("hasClipPath", true);
  }
  if (state_.pixel_moving_filter) {
    json->SetString("pixelMovingFilter",
                    String::Format("%p", state_.pixel_moving_filter.Get()));
  }
  return json;
}

}  // namespace blink
