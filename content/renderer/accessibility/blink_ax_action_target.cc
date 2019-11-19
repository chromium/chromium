// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_action_target.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/skia/include/core/SkMatrix44.h"

using blink::WebAXObject;
using blink::WebFloatRect;
using blink::WebPoint;
using blink::WebRect;

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

bool BlinkAXActionTarget::ClearAccessibilityFocus() const {
  return web_ax_object_.ClearAccessibilityFocus();
}

bool BlinkAXActionTarget::Click() const {
  return web_ax_object_.Click();
}

bool BlinkAXActionTarget::Decrement() const {
  return web_ax_object_.Decrement();
}

bool BlinkAXActionTarget::Increment() const {
  return web_ax_object_.Increment();
}

bool BlinkAXActionTarget::Focus() const {
  return web_ax_object_.Focus();
}

gfx::Rect BlinkAXActionTarget::GetRelativeBounds() const {
  blink::WebAXObject offset_container;
  WebFloatRect bounds;
  SkMatrix44 container_transform;
  web_ax_object_.GetRelativeBounds(offset_container, bounds,
                                   container_transform);
  return gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height);
}

gfx::Point BlinkAXActionTarget::GetScrollOffset() const {
  WebPoint offset = web_ax_object_.GetScrollOffset();
  return gfx::Point(offset.x, offset.y);
}

gfx::Point BlinkAXActionTarget::MinimumScrollOffset() const {
  WebPoint offset = web_ax_object_.MinimumScrollOffset();
  return gfx::Point(offset.x, offset.y);
}

gfx::Point BlinkAXActionTarget::MaximumScrollOffset() const {
  WebPoint offset = web_ax_object_.MaximumScrollOffset();
  return gfx::Point(offset.x, offset.y);
}

bool BlinkAXActionTarget::SetAccessibilityFocus() const {
  return web_ax_object_.SetAccessibilityFocus();
}

void BlinkAXActionTarget::SetScrollOffset(const gfx::Point& point) const {
  web_ax_object_.SetScrollOffset(WebPoint(point.x(), point.y()));
}

bool BlinkAXActionTarget::SetSelected(bool selected) const {
  return web_ax_object_.SetSelected(selected);
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

bool BlinkAXActionTarget::SetSequentialFocusNavigationStartingPoint() const {
  return web_ax_object_.SetSequentialFocusNavigationStartingPoint();
}

bool BlinkAXActionTarget::SetValue(const std::string& value) const {
  return web_ax_object_.SetValue(blink::WebString::FromUTF8(value));
}

bool BlinkAXActionTarget::ShowContextMenu() const {
  return web_ax_object_.ShowContextMenu();
}

bool BlinkAXActionTarget::ScrollToMakeVisible() const {
  return web_ax_object_.ScrollToMakeVisible();
}

bool BlinkAXActionTarget::ScrollToMakeVisibleWithSubFocus(
    const gfx::Rect& rect,
    ax::mojom::ScrollAlignment horizontal_scroll_alignment,
    ax::mojom::ScrollAlignment vertical_scroll_alignment) const {
  return web_ax_object_.ScrollToMakeVisibleWithSubFocus(
      WebRect(rect.x(), rect.y(), rect.width(), rect.height()),
      horizontal_scroll_alignment, vertical_scroll_alignment);
}

bool BlinkAXActionTarget::ScrollToGlobalPoint(const gfx::Point& point) const {
  return web_ax_object_.ScrollToGlobalPoint(WebPoint(point.x(), point.y()));
}

}  // namespace content
