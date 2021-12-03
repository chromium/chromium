// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"

#include "content/browser/accessibility/browser_accessibility_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManagerFuchsia(initial_tree, delegate);
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    content::BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManagerFuchsia(
      BrowserAccessibilityManagerFuchsia::GetEmptyDocument(), delegate);
}

BrowserAccessibilityManagerFuchsia::BrowserAccessibilityManagerFuchsia(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate)
    : BrowserAccessibilityManager(delegate) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerFuchsia::~BrowserAccessibilityManagerFuchsia() =
    default;

ui::AccessibilityBridgeFuchsia*
BrowserAccessibilityManagerFuchsia::GetAccessibilityBridge() const {
  auto* accessibility_bridge_registry =
      ui::AccessibilityBridgeFuchsiaRegistry::GetInstance();
  DCHECK(accessibility_bridge_registry);

  return accessibility_bridge_registry->GetAccessibilityBridge(ax_tree_id());
}

void BrowserAccessibilityManagerFuchsia::FireFocusEvent(
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireFocusEvent(node);

  if (!GetAccessibilityBridge())
    return;

  BrowserAccessibilityFuchsia* new_focus_fuchsia =
      ToBrowserAccessibilityFuchsia(node);

  BrowserAccessibilityFuchsia* old_focus_fuchsia =
      ToBrowserAccessibilityFuchsia(GetLastFocusedNode());

  if (old_focus_fuchsia) {
    GetAccessibilityBridge()->UnfocusNode(
        old_focus_fuchsia->GetFuchsiaNodeID());
  }

  if (new_focus_fuchsia) {
    GetAccessibilityBridge()->FocusNode(new_focus_fuchsia->GetFuchsiaNodeID());
  }
}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerFuchsia::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 1;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  empty_document.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

}  // namespace content
