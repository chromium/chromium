// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ACTION_TARGET_H_
#define CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ACTION_TARGET_H_

#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_action_target.h"

namespace content {

// Wraps a WebAXObject for dispatching accessibility actions.
class BlinkAXActionTarget : public ui::AXActionTarget {
 public:
  BlinkAXActionTarget(const blink::WebAXObject& web_ax_object);
  ~BlinkAXActionTarget() override;

  const blink::WebAXObject& WebAXObject() const;
  static const BlinkAXActionTarget* FromAXActionTarget(
      const ui::AXActionTarget* ax_action_target);

 protected:
  // AXActionTarget overrides.
  Type GetType() const override;
  bool PerformAction(const ui::AXActionData& action_data) const override;
  gfx::Rect GetRelativeBounds() const override;
  gfx::Point GetScrollOffset() const override;
  gfx::Point MinimumScrollOffset() const override;
  gfx::Point MaximumScrollOffset() const override;
  void SetScrollOffset(const gfx::Point& point) const override;
  bool SetSelection(const ui::AXActionTarget* anchor_object,
                    int anchor_offset,
                    const ui::AXActionTarget* focus_object,
                    int focus_offset) const override;
  bool ScrollToMakeVisible() const override;
  bool ScrollToMakeVisibleWithSubFocus(
      const gfx::Rect& rect,
      ax::mojom::ScrollAlignment horizontal_scroll_alignment,
      ax::mojom::ScrollAlignment vertical_scroll_alignment,
      ax::mojom::ScrollBehavior scroll_behavior) const override;

 private:
  blink::WebAXObject web_ax_object_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ACTION_TARGET_H_
