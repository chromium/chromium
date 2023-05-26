// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_action_target.h"
#include "third_party/blink/public/platform/web_string.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

using blink::WebAXObject;

namespace content {

BlinkAXActionTarget::BlinkAXActionTarget(
    const blink::WebAXObject& web_ax_object)
    : web_ax_object_(web_ax_object) {
  DCHECK(!web_ax_object.IsNull());
}

BlinkAXActionTarget::~BlinkAXActionTarget() {}

const WebAXObject& BlinkAXActionTarget::WebAXObject() const {
  return web_ax_object_;
}

// static
const BlinkAXActionTarget* BlinkAXActionTarget::FromAXActionTarget(
    const ui::AXActionTarget* ax_action_target) {
  if (ax_action_target->GetType() == ui::AXActionTarget::Type::kBlink)
    return static_cast<const BlinkAXActionTarget*>(ax_action_target);

  return nullptr;
}

ui::AXActionTarget::Type BlinkAXActionTarget::GetType() const {
  return ui::AXActionTarget::Type::kBlink;
}

bool BlinkAXActionTarget::PerformAction(
    const ui::AXActionData& action_data) const {
  return web_ax_object_.PerformAction(action_data);
}

gfx::Rect BlinkAXActionTarget::GetRelativeBounds() const {
  blink::WebAXObject offset_container;
  gfx::RectF bounds;
  gfx::Transform container_transform;
  web_ax_object_.GetRelativeBounds(offset_container, bounds,
                                   container_transform);
  return gfx::ToEnclosedRect(bounds);
}

gfx::Point BlinkAXActionTarget::GetScrollOffset() const {
  return web_ax_object_.GetScrollOffset();
}

gfx::Point BlinkAXActionTarget::MinimumScrollOffset() const {
  return web_ax_object_.MinimumScrollOffset();
}

gfx::Point BlinkAXActionTarget::MaximumScrollOffset() const {
  return web_ax_object_.MaximumScrollOffset();
}

void BlinkAXActionTarget::SetScrollOffset(const gfx::Point& point) const {
  web_ax_object_.SetScrollOffset(point);
}

bool BlinkAXActionTarget::SetSelection(const ui::AXActionTarget* anchor_object,
                                       int anchor_offset,
                                       const ui::AXActionTarget* focus_object,
                                       int focus_offset) const {
  const BlinkAXActionTarget* blink_anchor_object =
      BlinkAXActionTarget::FromAXActionTarget(anchor_object);
  const BlinkAXActionTarget* blink_focus_object =
      BlinkAXActionTarget::FromAXActionTarget(focus_object);
  if (!blink_anchor_object || !blink_focus_object)
    return false;

  return web_ax_object_.SetSelection(
      blink_anchor_object->WebAXObject(), anchor_offset,
      blink_focus_object->WebAXObject(), focus_offset);
}

bool BlinkAXActionTarget::ScrollToMakeVisible() const {
  return web_ax_object_.ScrollToMakeVisible();
}

bool BlinkAXActionTarget::ScrollToMakeVisibleWithSubFocus(
    const gfx::Rect& rect,
    ax::mojom::ScrollAlignment horizontal_scroll_alignment,
    ax::mojom::ScrollAlignment vertical_scroll_alignment,
    ax::mojom::ScrollBehavior scroll_behavior) const {
  return web_ax_object_.ScrollToMakeVisibleWithSubFocus(
      rect, horizontal_scroll_alignment, vertical_scroll_alignment,
      scroll_behavior);
}

}  // namespace content
