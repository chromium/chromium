// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/proto/main_content_extractor_proto_convertor.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "services/screen_ai/proto/view_hierarchy.pb.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace {

void AddAttribute(const std::string& name,
                  bool value,
                  screenai::UiElement& ui_element) {
  screenai::UiElementAttribute attrib;
  attrib.set_name(name);
  attrib.set_bool_value(value);
  ui_element.add_attributes()->Swap(&attrib);
}

void AddAttribute(const std::string& name,
                  int value,
                  screenai::UiElement& ui_element) {
  screenai::UiElementAttribute attrib;
  attrib.set_name(name);
  attrib.set_int_value(value);
  ui_element.add_attributes()->Swap(&attrib);
}

void AddAttribute(const std::string& name,
                  float value,
                  screenai::UiElement& ui_element) {
  screenai::UiElementAttribute attrib;
  attrib.set_name(name);
  attrib.set_float_value(value);
  ui_element.add_attributes()->Swap(&attrib);
}

template <class T>
void AddAttribute(const std::string& name,
                  const T& value,
                  screenai::UiElement& ui_element) {
  screenai::UiElementAttribute attrib;
  attrib.set_name(name);
  attrib.set_string_value(value);
  ui_element.add_attributes()->Swap(&attrib);
}

// Creates the proto for |node|, setting its own and parent id respectively to
// |id| and |parent_id|. Updates |tree_dimensions| to include the bounds of the
// new node.
// Requires setting "child_ids" and "bounding_box" properties in next steps.
screenai::UiElement CreateUiElementProto(const ui::AXTree& tree,
                                         const ui::AXNode* node,
                                         int id,
                                         int parent_id,
                                         gfx::SizeF& tree_dimensions) {
  screenai::UiElement uie;

  const ui::AXNodeData& node_data = node->data();

  // ID.
  uie.set_id(id);

  // Attributes.
  // TODO(https://crbug.com/40851192): Get attribute strings from a Google3
  // export, also the experimental ones for the unittest.
  AddAttribute("axnode_id", static_cast<int>(node->id()), uie);
  const std::string& display_value =
      node_data.GetStringAttribute(ax::mojom::StringAttribute::kDisplay);
  if (!display_value.empty())
    AddAttribute("/extras/styles/display", display_value, uie);
  AddAttribute("/extras/styles/visibility", !node_data.IsInvisible(), uie);

  // Add extra CSS attributes, such as text-align, hierarchical level, font
  // size, and font weight supported by both AXTree/AXNode and screen2x.
  // Screen2x expects these properties to be in the string format, so we
  // convert them into string.
  int32_t int_attribute_value;
  if (node_data.HasIntAttribute(ax::mojom::IntAttribute::kTextAlign)) {
    int_attribute_value =
        node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextAlign);
    AddAttribute("/extras/styles/text-align",
                 ui::ToString((ax::mojom::TextAlign)int_attribute_value), uie);
  }
  if (node_data.HasIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel)) {
    int_attribute_value =
        node_data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    AddAttribute("hierarchical_level", int_attribute_value, uie);
  }
  // Get float attributes and store them as string attributes in the screenai
  // proto for the main content extractor (screen2x).
  float float_attribute_value;
  if (node_data.HasFloatAttribute(ax::mojom::FloatAttribute::kFontSize)) {
    float_attribute_value =
        node_data.GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize);
    AddAttribute("/extras/styles/font-size", float_attribute_value, uie);
  }
  if (node_data.HasFloatAttribute(ax::mojom::FloatAttribute::kFontWeight)) {
    float_attribute_value =
        node_data.GetFloatAttribute(ax::mojom::FloatAttribute::kFontWeight);
    AddAttribute("/extras/styles/font-weight", float_attribute_value, uie);
  }

  // This is a fixed constant for Chrome requests to Screen2x.
  AddAttribute("class_name", "chrome.unicorn", uie);
  AddAttribute("chrome_role", ui::ToString(node_data.role), uie);
  AddAttribute("text",
               node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
               uie);

  // Type and parent.
  uie.set_parent_id(parent_id);

  // Type.
  uie.set_type(node == tree.root() ? screenai::UiElementType::ROOT
                                   : screenai::UiElementType::VIEW);

  // Bounding Box.
  gfx::RectF bounds = tree.RelativeToTreeBounds(
      node, gfx::RectF(0, 0),
      /* offscreen= */ nullptr, /* clip_bounds= */ false,
      /* skip_container_offset= */ false);

  // Bounding Box Pixels.
  screenai::BoundingBoxPixels* bounding_box_pixels =
      new screenai::BoundingBoxPixels();
  bounding_box_pixels->set_top(bounds.y());
  bounding_box_pixels->set_left(bounds.x());
  bounding_box_pixels->set_bottom(bounds.bottom());
  bounding_box_pixels->set_right(bounds.right());
  uie.set_allocated_bounding_box_pixels(bounding_box_pixels);

  tree_dimensions.set_height(fmax(tree_dimensions.height(), bounds.bottom()));
  tree_dimensions.set_width(fmax(tree_dimensions.width(), bounds.right()));

  return uie;
}

// Adds the subtree of |node| to |proto| with pre-order traversal.
// Uses |next_unused_node_id| as the current node id and updates it for the
// children. Updates |tree_dimensions| to include the bounds of the new node.
void AddSubTree(const ui::AXTree& tree,
                const ui::AXNode* node,
                screenai::ViewHierarchy& proto,
                int& next_unused_node_id,
                int parent_id,
                gfx::SizeF& tree_dimensions) {
  // Ensure that node id and index are the same.
  DCHECK(proto.ui_elements_size() == next_unused_node_id);

  // Create and add proto.
  int current_node_id = next_unused_node_id;
  screenai::UiElement uie = CreateUiElementProto(tree, node, current_node_id,
                                                 parent_id, tree_dimensions);
  proto.add_ui_elements()->Swap(&uie);

  // Add children.
  std::vector<int> child_ids;
  for (auto it = node->AllChildrenBegin(); it != node->AllChildrenEnd(); ++it) {
    child_ids.push_back(++next_unused_node_id);
    AddSubTree(tree, it.get(), proto, next_unused_node_id, current_node_id,
               tree_dimensions);
  }

  // Add child ids.
  for (int child : child_ids)
    proto.mutable_ui_elements(current_node_id)->add_child_ids(child);
}

}  // namespace

namespace screen_ai {

std::optional<ViewHierarchyAndTreeSize> SnapshotToViewHierarchy(
    const ui::AXTree& tree) {
  // Tree dimensions will be computed based on the max dimensions of all
  // elements in the tree.
  gfx::SizeF tree_dimensions;

  // Screen2x requires the nodes to come in PRE-ORDER, and have only positive
  // ids. |AddSubTree| traverses the |tree| in preorder and creates the
  // required proto.
  int next_unused_node_id = 0;
  screenai::ViewHierarchy proto;
  AddSubTree(tree, tree.root(), proto, next_unused_node_id, /*parent_id=*/-1,
             tree_dimensions);

  // If the tree has a zero dimension, there is nothing to send.
  if (tree_dimensions.IsEmpty())
    return std::nullopt;

  // The bounds of the root item should be set to the snapshot size.
  proto.mutable_ui_elements(0)->mutable_bounding_box_pixels()->set_right(
      tree_dimensions.width());
  proto.mutable_ui_elements(0)->mutable_bounding_box_pixels()->set_bottom(
      tree_dimensions.height());
  DCHECK_EQ(proto.ui_elements(0).bounding_box().right(), 0);
  DCHECK_EQ(proto.ui_elements(0).bounding_box().top(), 0);

  // Set relative sizes.
  for (int i = 0; i < proto.ui_elements_size(); i++) {
    auto* bounding_box = proto.mutable_ui_elements(i)->mutable_bounding_box();
    const auto& bounding_box_pixels =
        proto.ui_elements(i).bounding_box_pixels();
    bounding_box->set_top(bounding_box_pixels.top() / tree_dimensions.height());
    bounding_box->set_left(bounding_box_pixels.left() /
                           tree_dimensions.width());
    bounding_box->set_bottom(bounding_box_pixels.bottom() /
                             tree_dimensions.height());
    bounding_box->set_right(bounding_box_pixels.right() /
                            tree_dimensions.width());
  }

  return ViewHierarchyAndTreeSize{proto.SerializeAsString(), tree_dimensions};
}

}  // namespace screen_ai
