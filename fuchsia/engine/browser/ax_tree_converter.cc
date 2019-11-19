// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/ax_tree_converter.h"

#include <fuchsia/math/cpp/fidl.h>
#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/ui/scenic/cpp/commands.h>
#include <vector>

#include "base/bit_cast.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

fuchsia::accessibility::semantics::Role ConvertRole(ax::mojom::Role role) {
  if (role == ax::mojom::Role::kUnknown)
    return fuchsia::accessibility::semantics::Role::UNKNOWN;

  // TODO(crbug.com/973095): Currently Fuchsia only has one Role option. Add
  // more and update tests as they become supported.
  return fuchsia::accessibility::semantics::Role::UNKNOWN;
}

fuchsia::accessibility::semantics::Attributes SetAttributes(std::string name) {
  fuchsia::accessibility::semantics::Attributes attributes;
  attributes.set_label(name);
  return attributes;
}

std::vector<fuchsia::accessibility::semantics::Action> ConvertActions(
    uint32_t actions) {
  std::vector<fuchsia::accessibility::semantics::Action> fuchsia_actions;
  ax::mojom::Action action_enum = static_cast<ax::mojom::Action>(actions);

  switch (action_enum) {
    case ax::mojom::Action::kDoDefault:
      fuchsia_actions.push_back(
          fuchsia::accessibility::semantics::Action::DEFAULT);
      break;
    case ax::mojom::Action::kNone:
    case ax::mojom::Action::kAnnotatePageImages:
    case ax::mojom::Action::kBlur:
    case ax::mojom::Action::kClearAccessibilityFocus:
    case ax::mojom::Action::kCustomAction:
    case ax::mojom::Action::kDecrement:
    case ax::mojom::Action::kFocus:
    case ax::mojom::Action::kGetImageData:
    case ax::mojom::Action::kGetTextLocation:
    case ax::mojom::Action::kHideTooltip:
    case ax::mojom::Action::kHitTest:
    case ax::mojom::Action::kIncrement:
    case ax::mojom::Action::kInternalInvalidateTree:
    case ax::mojom::Action::kLoadInlineTextBoxes:
    case ax::mojom::Action::kReplaceSelectedText:
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollDown:
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollLeft:
    case ax::mojom::Action::kScrollRight:
    case ax::mojom::Action::kScrollUp:
    case ax::mojom::Action::kScrollToMakeVisible:
    case ax::mojom::Action::kScrollToPoint:
    case ax::mojom::Action::kSetAccessibilityFocus:
    case ax::mojom::Action::kSetScrollOffset:
    case ax::mojom::Action::kSetSelection:
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
    case ax::mojom::Action::kSetValue:
    case ax::mojom::Action::kShowContextMenu:
    case ax::mojom::Action::kSignalEndOfTest:
    case ax::mojom::Action::kShowTooltip:
      DVLOG(2) << "Action: " << action_enum;
      break;
  }

  return fuchsia_actions;
}

std::vector<uint32_t> ConvertChildIds(std::vector<int32_t> ids) {
  std::vector<uint32_t> child_ids;
  child_ids.reserve(ids.size());
  for (auto i : ids) {
    child_ids.push_back(bit_cast<uint32_t>(i));
  }
  return child_ids;
}

fuchsia::ui::gfx::BoundingBox ConvertBoundingBox(gfx::RectF bounds) {
  fuchsia::ui::gfx::BoundingBox box;
  float min[3] = {bounds.bottom_left().x(), bounds.bottom_left().y(), 0.0f};
  float max[3] = {bounds.top_right().x(), bounds.top_right().y(), 0.0f};
  box.min = scenic::NewVector3(min);
  box.max = scenic::NewVector3(max);
  return box;
}

// The Semantics Manager applies this matrix to position the node and its
// subtree as an optimization to handle resizing or repositioning. This requires
// only one node to be updated on such an event.
fuchsia::ui::gfx::mat4 ConvertTransform(gfx::Transform* transform) {
  float mat[16] = {};
  transform->matrix().asColMajorf(mat);
  fuchsia::ui::gfx::Matrix4Value fuchsia_transform =
      scenic::NewMatrix4Value(mat);
  return fuchsia_transform.value;
}

}  // namespace

fuchsia::accessibility::semantics::Node AXNodeDataToSemanticNode(
    const ui::AXNodeData& node) {
  fuchsia::accessibility::semantics::Node fuchsia_node;
  fuchsia_node.set_node_id(bit_cast<uint32_t>(node.id));
  fuchsia_node.set_role(ConvertRole(node.role));
  // TODO(fxb/18796): Handle node state conversions once available.
  if (node.HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    fuchsia_node.set_attributes(SetAttributes(
        node.GetStringAttribute(ax::mojom::StringAttribute::kName)));
  }
  fuchsia_node.set_actions(ConvertActions(node.actions));
  fuchsia_node.set_child_ids(ConvertChildIds(node.child_ids));
  fuchsia_node.set_location(ConvertBoundingBox(node.relative_bounds.bounds));
  if (node.relative_bounds.transform) {
    fuchsia_node.set_transform(
        ConvertTransform(node.relative_bounds.transform.get()));
  }

  return fuchsia_node;
}
