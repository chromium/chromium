// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_IOS_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_IOS_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/ax_platform_node_ios.h"

namespace content {

class BrowserAccessibilityIOS : public BrowserAccessibility,
                                public ui::AXPlatformNodeIOSDelegate {
 public:
  ~BrowserAccessibilityIOS() override;
  BrowserAccessibilityIOS(const BrowserAccessibilityIOS&) = delete;
  BrowserAccessibilityIOS& operator=(const BrowserAccessibilityIOS&) = delete;

  // BrowserAccessibility overrides.
  void OnDataChanged() override;
  size_t PlatformChildCount() const override;
  BrowserAccessibility* PlatformGetChild(size_t child_index) const override;
  BrowserAccessibility* PlatformGetFirstChild() const override;
  BrowserAccessibility* PlatformGetLastChild() const override;
  BrowserAccessibility* PlatformGetNextSibling() const override;
  BrowserAccessibility* PlatformGetPreviousSibling() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::AXPlatformNode* GetAXPlatformNode() const override;

  // ui::AXPlatformNodeIOSDelegate overrides.
  float GetDeviceScaleFactor() const override;

 protected:
  BrowserAccessibilityIOS(BrowserAccessibilityManager* manager,
                          ui::AXNode* node);

  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  // Creates platform node if not yet created.
  void CreatePlatformNode();

  // Manager of the native wrapper node. This should be a unique_ptr but
  // currently AXPlatformNodeBase manually manages deleting itself.
  raw_ptr<ui::AXPlatformNodeIOS> platform_node_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_IOS_H_
