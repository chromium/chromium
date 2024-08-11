// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"

#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

WTF::String OverscrollBehaviorTypeToString(cc::OverscrollBehavior::Type value) {
  switch (value) {
    case cc::OverscrollBehavior::Type::kNone:
      return "none";
    case cc::OverscrollBehavior::Type::kAuto:
      return "auto";
    case cc::OverscrollBehavior::Type::kContain:
      return "contain";
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace

PaintPropertyChangeType ScrollPaintPropertyNode::State::ComputeChange(
    const State& other) const {
  if (container_rect != other.container_rect ||
      contents_size != other.contents_size ||
      overflow_clip_node != other.overflow_clip_node ||
      user_scrollable_horizontal != other.user_scrollable_horizontal ||
      user_scrollable_vertical != other.user_scrollable_vertical ||
      prevent_viewport_scrolling_from_inner !=
          other.prevent_viewport_scrolling_from_inner ||
      max_scroll_offset_affected_by_page_scale !=
          other.max_scroll_offset_affected_by_page_scale ||
      composited_scrolling_preference !=
          other.composited_scrolling_preference ||
      main_thread_scrolling_reasons != other.main_thread_scrolling_reasons ||
      compositor_element_id != other.compositor_element_id ||
      overscroll_behavior != other.overscroll_behavior ||
      snap_container_data != other.snap_container_data) {
    return PaintPropertyChangeType::kChangedOnlyValues;
  }
  return PaintPropertyChangeType::kUnchanged;
}

void ScrollPaintPropertyNode::State::Trace(Visitor* visitor) const {
  visitor->Trace(overflow_clip_node);
}

ScrollPaintPropertyNode::ScrollPaintPropertyNode(RootTag)
    : PaintPropertyNodeBase(kRoot),
      state_{InfiniteIntRect(), InfiniteIntRect().size()} {}

const ScrollPaintPropertyNode& ScrollPaintPropertyNode::Root() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollPaintPropertyNode>, root,
                      (MakeGarbageCollected<ScrollPaintPropertyNode>(kRoot)));
  return *root;
}

void ScrollPaintPropertyNode::ClearChangedToRoot(int sequence_number) const {
  for (auto* n = this; n && n->ChangedSequenceNumber() != sequence_number;
       n = n->Parent()) {
    n->ClearChanged(sequence_number);
  }
}

std::unique_ptr<JSONObject> ScrollPaintPropertyNode::ToJSON() const {
  auto json = PaintPropertyNode::ToJSON();
  if (!state_.container_rect.IsEmpty())
    json->SetString("containerRect", String(state_.container_rect.ToString()));
  if (!state_.contents_size.IsEmpty())
    json->SetString("contentsSize", String(state_.contents_size.ToString()));
  if (state_.overflow_clip_node) {
    json->SetString("overflowClipNode",
                    String::Format("%p", state_.overflow_clip_node.Get()));
  }
  if (state_.user_scrollable_horizontal || state_.user_scrollable_vertical) {
    json->SetString(
        "userScrollable",
        state_.user_scrollable_horizontal
            ? (state_.user_scrollable_vertical ? "both" : "horizontal")
            : "vertical");
  }
  if (state_.main_thread_scrolling_reasons) {
    json->SetString("mainThreadReasons",
                    cc::MainThreadScrollingReason::AsText(
                        state_.main_thread_scrolling_reasons)
                        .c_str());
  }
  if (state_.max_scroll_offset_affected_by_page_scale)
    json->SetString("maxScrollOffsetAffectedByPageScale", "true");
  if (state_.compositor_element_id) {
    json->SetString("compositorElementId",
                    state_.compositor_element_id.ToString().c_str());
  }
  if (state_.overscroll_behavior.x != cc::OverscrollBehavior::Type::kAuto) {
    json->SetString("overscroll-behavior-x", OverscrollBehaviorTypeToString(
                                                 state_.overscroll_behavior.x));
  }
  if (state_.overscroll_behavior.y != cc::OverscrollBehavior::Type::kAuto) {
    json->SetString("overscroll-behavior-y", OverscrollBehaviorTypeToString(
                                                 state_.overscroll_behavior.y));
  }

  if (state_.snap_container_data) {
    json->SetString("snap_container_rect",
                    state_.snap_container_data->rect().ToString().c_str());
    if (state_.snap_container_data->size()) {
      auto area_rects_json = std::make_unique<JSONArray>();
      for (size_t i = 0; i < state_.snap_container_data->size(); ++i) {
        area_rects_json->PushString(
            state_.snap_container_data->at(i).rect.ToString().c_str());
      }
      json->SetArray("snap_area_rects", std::move(area_rects_json));
    }
  }

  return json;
}

}  // namespace blink
