// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_enum_conversion.h"

#include "base/logging.h"

namespace content {

void AXStateFromBlink(const blink::WebAXObject& o, ui::AXNodeData* dst) {
  blink::WebAXExpanded expanded = o.IsExpanded();
  if (expanded) {
    if (expanded == blink::kWebAXExpandedCollapsed)
      dst->AddState(ax::mojom::State::kCollapsed);
    else if (expanded == blink::kWebAXExpandedExpanded)
      dst->AddState(ax::mojom::State::kExpanded);
  }

  if (o.CanSetFocusAttribute())
    dst->AddState(ax::mojom::State::kFocusable);

  if (o.HasPopup() != ax::mojom::HasPopup::kFalse)
    dst->SetHasPopup(o.HasPopup());
  else if (o.Role() == ax::mojom::Role::kPopUpButton)
    dst->SetHasPopup(ax::mojom::HasPopup::kMenu);

  if (o.IsAutofillAvailable())
    dst->AddState(ax::mojom::State::kAutofillAvailable);

  if (o.IsDefault())
    dst->AddState(ax::mojom::State::kDefault);

  // aria-grabbed is deprecated in WAI-ARIA 1.1.
  if (o.IsGrabbed() != blink::kWebAXGrabbedStateUndefined)
    dst->AddBoolAttribute(ax::mojom::BoolAttribute::kGrabbed,
                          o.IsGrabbed() == blink::kWebAXGrabbedStateTrue);

  if (o.IsHovered())
    dst->AddState(ax::mojom::State::kHovered);

  if (!o.IsVisible())
    dst->AddState(ax::mojom::State::kInvisible);

  if (o.IsLinked())
    dst->AddState(ax::mojom::State::kLinked);

  if (o.IsMultiline())
    dst->AddState(ax::mojom::State::kMultiline);

  if (o.IsMultiSelectable())
    dst->AddState(ax::mojom::State::kMultiselectable);

  if (o.IsPasswordField())
    dst->AddState(ax::mojom::State::kProtected);

  if (o.IsRequired())
    dst->AddState(ax::mojom::State::kRequired);

  if (o.IsEditable())
    dst->AddState(ax::mojom::State::kEditable);

  if (o.IsSelected() != blink::kWebAXSelectedStateUndefined) {
    dst->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                          o.IsSelected() == blink::kWebAXSelectedStateTrue);
  }

  if (o.IsRichlyEditable())
    dst->AddState(ax::mojom::State::kRichlyEditable);

  if (o.IsVisited())
    dst->AddState(ax::mojom::State::kVisited);

  if (o.Orientation() == blink::kWebAXOrientationVertical)
    dst->AddState(ax::mojom::State::kVertical);
  else if (o.Orientation() == blink::kWebAXOrientationHorizontal)
    dst->AddState(ax::mojom::State::kHorizontal);

  if (o.AccessibilityIsIgnored())
    dst->AddState(ax::mojom::State::kIgnored);
}

}  // namespace content.
