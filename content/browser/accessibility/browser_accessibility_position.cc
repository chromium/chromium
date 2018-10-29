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

namespace content {

BrowserAccessibilityPosition::BrowserAccessibilityPosition() {}

BrowserAccessibilityPosition::~BrowserAccessibilityPosition() {}

BrowserAccessibilityPosition::AXPositionInstance
BrowserAccessibilityPosition::Clone() const {
  return AXPositionInstance(new BrowserAccessibilityPosition(*this));
}

base::string16 BrowserAccessibilityPosition::GetInnerText() const {
  if (IsNullPosition())
    return base::string16();
  DCHECK(GetAnchor());
  return GetAnchor()->GetText();
}

void BrowserAccessibilityPosition::AnchorChild(int child_index,
                                               AXTreeID* tree_id,
                                               int32_t* child_id) const {
  DCHECK(tree_id);
  DCHECK(child_id);

  if (!GetAnchor() || child_index < 0 || child_index >= AnchorChildCount()) {
    *tree_id = ui::AXTreeIDUnknown();
    *child_id = INVALID_ANCHOR_ID;
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

int BrowserAccessibilityPosition::AnchorIndexInParent() const {
  return GetAnchor() ? static_cast<int>(GetAnchor()->GetIndexInParent())
                     : AXPosition::INVALID_INDEX;
}

void BrowserAccessibilityPosition::AnchorParent(AXTreeID* tree_id,
                                                int32_t* parent_id) const {
  DCHECK(tree_id);
  DCHECK(parent_id);

  if (!GetAnchor() || !GetAnchor()->PlatformGetParent()) {
    *tree_id = ui::AXTreeIDUnknown();
    *parent_id = AXPosition::INVALID_ANCHOR_ID;
    return;
  }

  BrowserAccessibility* parent = GetAnchor()->PlatformGetParent();
  *tree_id = parent->manager()->ax_tree_id();
  *parent_id = parent->GetId();
}

BrowserAccessibility* BrowserAccessibilityPosition::GetNodeInTree(
    AXTreeID tree_id,
    int32_t node_id) const {
  if (tree_id == ui::AXTreeIDUnknown() ||
      node_id == AXPosition::INVALID_ANCHOR_ID) {
    return nullptr;
  }

  auto* manager = BrowserAccessibilityManager::FromID(tree_id);
  if (!manager)
    return nullptr;
  return manager->GetFromID(node_id);
}

int BrowserAccessibilityPosition::MaxTextOffset() const {
  if (IsNullPosition())
    return INVALID_OFFSET;
  return static_cast<int>(GetInnerText().length());
}

// On some platforms, most objects are represented in the text of their parents
// with a special (embedded object) character and not with their actual text
// contents.
int BrowserAccessibilityPosition::MaxTextOffsetInParent() const {
#if defined(OS_WIN) || BUILDFLAG(USE_ATK)
  if (IsNullPosition())
    return INVALID_OFFSET;
  if (GetAnchor()->IsTextOnlyObject())
    return MaxTextOffset();
  // Not all objects in the internal accessibility tree are exposed to platform
  // APIs.
  if (GetAnchor()->PlatformIsChildOfLeaf())
    return MaxTextOffset();
  return 1;
#else
  return MaxTextOffset();
#endif
}

bool BrowserAccessibilityPosition::IsInWhiteSpace() const {
  if (IsNullPosition())
    return false;

  DCHECK(GetAnchor());
  return GetAnchor()->IsLineBreakObject() ||
         base::ContainsOnlyChars(GetInnerText(), base::kWhitespaceUTF16);
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

int32_t BrowserAccessibilityPosition::GetNextOnLineID(int32_t node_id) const {
  if (IsNullPosition())
    return INVALID_ANCHOR_ID;
  BrowserAccessibility* node = GetNodeInTree(tree_id(), node_id);
  int next_on_line_id;
  if (!node || !node->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                      &next_on_line_id)) {
    return INVALID_ANCHOR_ID;
  }
  return static_cast<int32_t>(next_on_line_id);
}

int32_t BrowserAccessibilityPosition::GetPreviousOnLineID(
    int32_t node_id) const {
  if (IsNullPosition())
    return INVALID_ANCHOR_ID;
  BrowserAccessibility* node = GetNodeInTree(tree_id(), node_id);
  int previous_on_line_id;
  if (!node ||
      !node->GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                             &previous_on_line_id)) {
    return INVALID_ANCHOR_ID;
  }
  return static_cast<int32_t>(previous_on_line_id);
}

}  // namespace content
