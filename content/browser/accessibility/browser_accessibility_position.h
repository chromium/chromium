// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_POSITION_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_POSITION_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_tree_id_registry.h"

namespace content {

class BrowserAccessibility;

using AXTreeID = ui::AXTreeID;

class CONTENT_EXPORT BrowserAccessibilityPosition
    : public ui::AXPosition<BrowserAccessibilityPosition,
                            BrowserAccessibility> {
 public:
  BrowserAccessibilityPosition();
  ~BrowserAccessibilityPosition() override;
  BrowserAccessibilityPosition(const BrowserAccessibilityPosition& other);

  AXPositionInstance Clone() const override;

  base::string16 GetText() const override;
  bool IsInLineBreak() const override;
  bool IsInTextObject() const override;
  bool IsInWhiteSpace() const override;

 protected:
  void AnchorChild(int child_index,
                   AXTreeID* tree_id,
                   ui::AXNode::AXID* child_id) const override;
  int AnchorChildCount() const override;
  int AnchorIndexInParent() const override;
  base::stack<BrowserAccessibility*> GetAncestorAnchors() const override;
  void AnchorParent(AXTreeID* tree_id,
                    ui::AXNode::AXID* parent_id) const override;
  BrowserAccessibility* GetNodeInTree(AXTreeID tree_id,
                                      ui::AXNode::AXID node_id) const override;
  bool IsEmbeddedObjectInParent() const override;

  bool IsInLineBreakingObject() const override;
  ax::mojom::Role GetRole() const override;
  ui::AXNodeTextStyles GetTextStyles() const override;
  std::vector<int32_t> GetWordStartOffsets() const override;
  std::vector<int32_t> GetWordEndOffsets() const override;
  ui::AXNode::AXID GetNextOnLineID(ui::AXNode::AXID node_id) const override;
  ui::AXNode::AXID GetPreviousOnLineID(ui::AXNode::AXID node_id) const override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_POSITION_H_
