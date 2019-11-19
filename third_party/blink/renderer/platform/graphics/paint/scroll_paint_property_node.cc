// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"

namespace blink {

namespace {

WTF::String OverscrollBehaviorTypeToString(
    cc::OverscrollBehavior::OverscrollBehaviorType value) {
  switch (value) {
    case cc::OverscrollBehavior::kOverscrollBehaviorTypeNone:
      return "none";
    case cc::OverscrollBehavior::kOverscrollBehaviorTypeAuto:
      return "auto";
    case cc::OverscrollBehavior::kOverscrollBehaviorTypeContain:
      return "contain";
    default:
      NOTREACHED();
  }
}

}  // namespace

const ScrollPaintPropertyNode& ScrollPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(
      ScrollPaintPropertyNode, root,
      base::AdoptRef(new ScrollPaintPropertyNode(nullptr, State{})));
  return *root;
}

std::unique_ptr<JSONObject> ScrollPaintPropertyNode::ToJSON() const {
  auto json = std::make_unique<JSONObject>();
  if (Parent())
    json->SetString("parent", String::Format("%p", Parent()));
  if (state_.container_rect != IntRect())
    json->SetString("containerRect", state_.container_rect.ToString());
  if (!state_.contents_size.IsZero())
    json->SetString("contentsSize", state_.contents_size.ToString());
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
  if (state_.scrolls_inner_viewport)
    json->SetString("scrollsInnerViewport", "true");
  if (state_.scrolls_outer_viewport)
    json->SetString("scrollsOuterViewport", "true");
  if (state_.max_scroll_offset_affected_by_page_scale)
    json->SetString("maxScrollOffsetAffectedByPageScale", "true");
  if (state_.compositor_element_id) {
    json->SetString("compositorElementId",
                    state_.compositor_element_id.ToString().c_str());
  }
  if (state_.overscroll_behavior.x !=
      cc::OverscrollBehavior::kOverscrollBehaviorTypeAuto) {
    json->SetString("overscroll-behavior-x", OverscrollBehaviorTypeToString(
                                                 state_.overscroll_behavior.x));
  }
  if (state_.overscroll_behavior.y !=
      cc::OverscrollBehavior::kOverscrollBehaviorTypeAuto) {
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
