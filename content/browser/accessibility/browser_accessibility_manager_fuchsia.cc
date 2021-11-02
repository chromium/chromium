// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"

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
    : BrowserAccessibilityManager(initial_tree, delegate) {}

BrowserAccessibilityManagerFuchsia::~BrowserAccessibilityManagerFuchsia() =
    default;

void BrowserAccessibilityManagerFuchsia::FireFocusEvent(
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireFocusEvent(node);
  // TODO(fxb.dev/82826): Send updates to fuchsia for the newly and previously
  // focused nodes.
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
