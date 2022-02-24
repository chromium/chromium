// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"

#include <lib/sys/inspect/cpp/component.h>

#include "content/browser/accessibility/browser_accessibility_fuchsia.h"
#include "content/public/browser/web_contents.h"
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

  ui::AccessibilityBridgeFuchsia* accessibility_bridge =
      GetAccessibilityBridge();
  if (accessibility_bridge) {
    inspect_node_ = accessibility_bridge->GetInspectNode();
    tree_dump_node_ = inspect_node_.CreateLazyNode("tree-data", [this]() {
      inspect::Inspector inspector;

      inspector.GetRoot().CreateString(ax_tree_id().ToString(),
                                       ax_tree()->ToString(), &inspector);

      return fpromise::make_ok_promise(inspector);
    });
  }
}

BrowserAccessibilityManagerFuchsia::~BrowserAccessibilityManagerFuchsia() =
    default;

ui::AccessibilityBridgeFuchsia*
BrowserAccessibilityManagerFuchsia::GetAccessibilityBridge() const {
  if (accessibility_bridge_for_test_)
    return accessibility_bridge_for_test_;

  ui::AccessibilityBridgeFuchsiaRegistry* accessibility_bridge_registry =
      ui::AccessibilityBridgeFuchsiaRegistry::GetInstance();
  DCHECK(accessibility_bridge_registry);

  WebContents* web_contents = this->web_contents();
  if (!web_contents)
    return nullptr;

  gfx::NativeWindow top_level_native_window =
      web_contents->GetTopLevelNativeWindow();
  if (!top_level_native_window)
    return nullptr;

  aura::Window* root_window = top_level_native_window->GetRootWindow();
  if (!root_window)
    return nullptr;

  return accessibility_bridge_registry->GetAccessibilityBridge(root_window);
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

  if (old_focus_fuchsia)
    old_focus_fuchsia->OnDataChanged();

  if (new_focus_fuchsia)
    new_focus_fuchsia->OnDataChanged();
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

void BrowserAccessibilityManagerFuchsia::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node,
    int action_request_id) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node,
                                              action_request_id);

  // Blink fires a hover event on the result of a hit test, so we should process
  // it here.
  if (event_type == ax::mojom::Event::kHover)
    OnHitTestResult(action_request_id, node);
}

void BrowserAccessibilityManagerFuchsia::OnHitTestResult(
    int action_request_id,
    BrowserAccessibility* node) {
  if (!GetAccessibilityBridge())
    return;

  absl::optional<uint32_t> hit_result_id;

  if (node) {
    BrowserAccessibilityFuchsia* hit_result =
        ToBrowserAccessibilityFuchsia(node);
    DCHECK(hit_result);
    hit_result_id = hit_result->GetFuchsiaNodeID();
  }

  GetAccessibilityBridge()->OnAccessibilityHitTestResult(action_request_id,
                                                         hit_result_id);
}

void BrowserAccessibilityManagerFuchsia::UpdateDeviceScaleFactor() {
  ui::AccessibilityBridgeFuchsia* accessibility_bridge =
      GetAccessibilityBridge();
  if (accessibility_bridge)
    device_scale_factor_ = accessibility_bridge->GetDeviceScaleFactor();
  else
    BrowserAccessibilityManager::UpdateDeviceScaleFactor();
}

void BrowserAccessibilityManagerFuchsia::SetAccessibilityBridgeForTest(
    ui::AccessibilityBridgeFuchsia* accessibility_bridge_for_test) {
  accessibility_bridge_for_test_ = accessibility_bridge_for_test;
}

}  // namespace content
