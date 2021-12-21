// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"

namespace content {

class BrowserAccessibilityFuchsia;

// Manages a tree of BrowserAccessibilityFuchsia objects.
class CONTENT_EXPORT BrowserAccessibilityManagerFuchsia
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerFuchsia(const ui::AXTreeUpdate& initial_tree,
                                     BrowserAccessibilityDelegate* delegate);
  ~BrowserAccessibilityManagerFuchsia() override;

  BrowserAccessibilityManagerFuchsia(
      const BrowserAccessibilityManagerFuchsia&) = delete;
  BrowserAccessibilityManagerFuchsia& operator=(
      const BrowserAccessibilityManagerFuchsia&) = delete;

  static ui::AXTreeUpdate GetEmptyDocument();

  // BrowserAccessibilityManager overrides.
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void FireFocusEvent(BrowserAccessibility* node) override;
  float device_scale_factor() const override;

  // Sends hit test result to fuchsia.
  void OnHitTestResult(int action_request_id, BrowserAccessibility* node);

 private:
  // Returns the accessibility bridge instance for this manager's WebContents.
  ui::AccessibilityBridgeFuchsia* GetAccessibilityBridge() const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_FUCHSIA_H_
