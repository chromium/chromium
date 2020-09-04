// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_position.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_buildflags.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace content {

BrowserAccessibilityPosition::BrowserAccessibilityPosition() = default;

BrowserAccessibilityPosition::~BrowserAccessibilityPosition() = default;

BrowserAccessibilityPosition::BrowserAccessibilityPosition(
    const BrowserAccessibilityPosition& other)
    : ui::AXPosition<BrowserAccessibilityPosition, BrowserAccessibility>(
          other) {}

BrowserAccessibilityPosition::AXPositionInstance
BrowserAccessibilityPosition::Clone() const {
  return AXPositionInstance(new BrowserAccessibilityPosition(*this));
}

base::string16 BrowserAccessibilityPosition::GetText() const {
  if (IsNullPosition())
    return {};
  DCHECK(GetAnchor());
  return GetAnchor()->GetText();
}

bool BrowserAccessibilityPosition::IsInLineBreak() const {
  if (IsNullPosition())
    return false;
  DCHECK(GetAnchor());
  return GetAnchor()->IsLineBreakObject();
}

bool BrowserAccessibilityPosition::IsInTextObject() const {
  if (IsNullPosition())
    return false;
  DCHECK(GetAnchor());
  return GetAnchor()->IsText();
}

bool BrowserAccessibilityPosition::IsInWhiteSpace() const {
  if (IsNullPosition())
    return false;
  DCHECK(GetAnchor());
  return GetAnchor()->IsLineBreakObject() ||
         base::ContainsOnlyChars(GetText(), base::kWhitespaceUTF16);
}

void BrowserAccessibilityPosition::AnchorChild(
    int child_index,
    AXTreeID* tree_id,
    ui::AXNode::AXID* child_id) const {
  DCHECK(tree_id);
  DCHECK(child_id);

  if (!GetAnchor() || child_index < 0 || child_index >= AnchorChildCount()) {
    *tree_id = ui::AXTreeIDUnknown();
    *child_id = ui::AXNode::kInvalidAXID;
    return;
  }

  BrowserAccessibility* child = nullptr;
  if (GetAnchor()->PlatformIsLeaf()) {
    child = GetAnchor()->InternalGetChild(child_index);
  } else {
    child = GetAnchor()->PlatformGetChild(child_index);
  }
  DCHECK(child);
  *tree_id = child->manager()->ax_tree_id();
  *child_id = child->GetId();
}

int BrowserAccessibilityPosition::AnchorChildCount() const {
  if (!GetAnchor())
    return 0;

  if (GetAnchor()->PlatformIsLeaf()) {
    return static_cast<int>(GetAnchor()->InternalChildCount());
  } else {
    return static_cast<int>(GetAnchor()->PlatformChildCount());
  }
}

int BrowserAccessibilityPosition::AnchorUnignoredChildCount() const {
  if (!GetAnchor())
    return 0;

  return static_cast<int>(GetAnchor()->InternalChildCount());
}

int BrowserAccessibilityPosition::AnchorIndexInParent() const {
  return GetAnchor() ? GetAnchor()->GetIndexInParent()
                     : AXPosition::INVALID_INDEX;
}

int BrowserAccessibilityPosition::AnchorSiblingCount() const {
  BrowserAccessibility* parent = GetAnchor()->PlatformGetParent();
  if (parent)
    return static_cast<int>(parent->InternalChildCount());
  return 0;
}

base::stack<BrowserAccessibility*>
BrowserAccessibilityPosition::GetAncestorAnchors() const {
  base::stack<BrowserAccessibility*> anchors;
  BrowserAccessibility* current_anchor = GetAnchor();
  while (current_anchor) {
    anchors.push(current_anchor);
    current_anchor = current_anchor->PlatformGetParent();
  }
  return anchors;
}

BrowserAccessibility* BrowserAccessibilityPosition::GetLowestUnignoredAncestor()
    const {
  if (!GetAnchor())
    return nullptr;

  return GetAnchor()->PlatformGetParent();
}

void BrowserAccessibilityPosition::AnchorParent(
    AXTreeID* tree_id,
    ui::AXNode::AXID* parent_id) const {
  DCHECK(tree_id);
  DCHECK(parent_id);

  if (!GetAnchor() || !GetAnchor()->PlatformGetParent()) {
    *tree_id = ui::AXTreeIDUnknown();
    *parent_id = ui::AXNode::kInvalidAXID;
    return;
  }

  BrowserAccessibility* parent = GetAnchor()->PlatformGetParent();
  *tree_id = parent->manager()->ax_tree_id();
  *parent_id = parent->GetId();
}

BrowserAccessibility* BrowserAccessibilityPosition::GetNodeInTree(
    AXTreeID tree_id,
    ui::AXNode::AXID node_id) const {
  if (tree_id == ui::AXTreeIDUnknown() || node_id == ui::AXNode::kInvalidAXID) {
    return nullptr;
  }

  auto* manager = BrowserAccessibilityManager::FromID(tree_id);
  if (!manager)
    return nullptr;
  return manager->GetFromID(node_id);
}

int32_t BrowserAccessibilityPosition::GetAnchorID(
    BrowserAccessibility* node) const {
  return node->GetId();
}

AXTreeID BrowserAccessibilityPosition::GetTreeID(
    BrowserAccessibility* node) const {
  return node->manager()->ax_tree_id();
}

bool BrowserAccessibilityPosition::IsEmbeddedObjectInParent() const {
  // On some platforms, most objects are represented in the text of their
  // parents with a special (embedded object) character and not with their
  // actual text contents.
#if defined(OS_WIN) || BUILDFLAG(USE_ATK)
  // Not all objects in the internal accessibility tree are exposed to platform
  // APIs.
  return !IsNullPosition() && !GetAnchor()->IsText() &&
         !GetAnchor()->IsChildOfLeaf();
#else
  return false;
#endif
}

bool BrowserAccessibilityPosition::IsInLineBreakingObject() const {
  if (IsNullPosition())
    return false;
  DCHECK(GetAnchor());
  return GetAnchor()->GetBoolAttribute(
             ax::mojom::BoolAttribute::kIsLineBreakingObject) &&
         !GetAnchor()->IsInListMarker();
}

ax::mojom::Role BrowserAccessibilityPosition::GetAnchorRole() const {
  if (IsNullPosition())
    return ax::mojom::Role::kNone;
  DCHECK(GetAnchor());
  return GetRole(GetAnchor());
}

ax::mojom::Role BrowserAccessibilityPosition::GetRole(
    BrowserAccessibility* node) const {
  return node->GetRole();
}

ui::AXNodeTextStyles BrowserAccessibilityPosition::GetTextStyles() const {
  // Check either the current anchor or its parent for text styles.
  ui::AXNodeTextStyles current_anchor_text_styles =
      !IsNullPosition() ? GetAnchor()->GetData().GetTextStyles()
                        : ui::AXNodeTextStyles();
  if (current_anchor_text_styles.IsUnset()) {
    AXPositionInstance parent = CreateParentPosition();
    if (!parent->IsNullPosition())
      return parent->GetAnchor()->GetData().GetTextStyles();
  }
  return current_anchor_text_styles;
}

std::vector<int32_t> BrowserAccessibilityPosition::GetWordStartOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());
  return GetAnchor()->GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts);
}

std::vector<int32_t> BrowserAccessibilityPosition::GetWordEndOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());
  return GetAnchor()->GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordEnds);
}

ui::AXNode::AXID BrowserAccessibilityPosition::GetNextOnLineID(
    ui::AXNode::AXID node_id) const {
  if (IsNullPosition())
    return ui::AXNode::kInvalidAXID;
  BrowserAccessibility* node = GetNodeInTree(tree_id(), node_id);
  int next_on_line_id;
  if (!node || !node->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                      &next_on_line_id)) {
    return ui::AXNode::kInvalidAXID;
  }
  return static_cast<ui::AXNode::AXID>(next_on_line_id);
}

ui::AXNode::AXID BrowserAccessibilityPosition::GetPreviousOnLineID(
    ui::AXNode::AXID node_id) const {
  if (IsNullPosition())
    return ui::AXNode::kInvalidAXID;
  BrowserAccessibility* node = GetNodeInTree(tree_id(), node_id);
  int previous_on_line_id;
  if (!node ||
      !node->GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                             &previous_on_line_id)) {
    return ui::AXNode::kInvalidAXID;
  }
  return static_cast<ui::AXNode::AXID>(previous_on_line_id);
}

}  // namespace content
