// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_POSITION_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_POSITION_H_

#include <stdint.h>

#include <vector>

#include "base/strings/string16.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_tree_id_registry.h"

namespace content {

class BrowserAccessibility;

using AXTreeID = ui::AXTreeID;

class BrowserAccessibilityPosition
    : public ui::AXPosition<BrowserAccessibilityPosition,
                            BrowserAccessibility> {
 public:
  BrowserAccessibilityPosition();
  ~BrowserAccessibilityPosition() override;

  AXPositionInstance Clone() const override;

  base::string16 GetInnerText() const override;

 protected:
  BrowserAccessibilityPosition(const BrowserAccessibilityPosition& other) =
      default;
  void AnchorChild(int child_index,
                   AXTreeID* tree_id,
                   int32_t* child_id) const override;
  int AnchorChildCount() const override;
  int AnchorIndexInParent() const override;
  void AnchorParent(AXTreeID* tree_id, int32_t* parent_id) const override;
  BrowserAccessibility* GetNodeInTree(AXTreeID tree_id,
                                      int32_t node_id) const override;
  int MaxTextOffset() const override;
  int MaxTextOffsetInParent() const override;
  bool IsInWhiteSpace() const override;
  std::vector<int32_t> GetWordStartOffsets() const override;
  std::vector<int32_t> GetWordEndOffsets() const override;
  int32_t GetNextOnLineID(int32_t node_id) const override;
  int32_t GetPreviousOnLineID(int32_t node_id) const override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_POSITION_H_
