// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_

#include <lib/inspect/cpp/vmo/types.h>

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"

namespace content {

class BrowserAccessibilityFuchsia;
class WebAXPlatformTreeManagerDelegate;

// Manages a tree of BrowserAccessibilityFuchsia objects.
class CONTENT_EXPORT BrowserAccessibilityManagerFuchsia
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerFuchsia(
      const ui::AXTreeUpdate& initial_tree,
      WebAXPlatformTreeManagerDelegate* delegate);
  ~BrowserAccessibilityManagerFuchsia() override;

  BrowserAccessibilityManagerFuchsia(
      const BrowserAccessibilityManagerFuchsia&) = delete;
  BrowserAccessibilityManagerFuchsia& operator=(
      const BrowserAccessibilityManagerFuchsia&) = delete;

  static ui::AXTreeUpdate GetEmptyDocument();

  // AXTreeManager override.
  void FireFocusEvent(ui::AXNode* node) override;

  // BrowserAccessibilityManager overrides.
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void UpdateDeviceScaleFactor() override;

  // Sends hit test result to fuchsia.
  void OnHitTestResult(int action_request_id, BrowserAccessibility* node);

  // Returns the accessibility bridge instance for this manager's native window.
  ui::AccessibilityBridgeFuchsia* GetAccessibilityBridge() const;

  // Test-only method to set the return value of GetAccessibilityBridge().
  void SetAccessibilityBridgeForTest(
      ui::AccessibilityBridgeFuchsia* accessibility_bridge_for_test);

 private:
  // Accessibility bridge instance to use for tests, if set.
  ui::AccessibilityBridgeFuchsia* accessibility_bridge_for_test_ = nullptr;

  // Node to hold this object fuchsia inspect data.
  inspect::Node inspect_node_;

  // Node to output a dump of this object's AXTree.
  inspect::LazyNode tree_dump_node_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_
