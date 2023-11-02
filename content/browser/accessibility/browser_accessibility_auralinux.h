// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node.h"

namespace ui {

class AXPlatformNodeAuraLinux;

}  // namespace ui

namespace content {

class BrowserAccessibilityAuraLinux : public BrowserAccessibility {
 public:
  BrowserAccessibilityAuraLinux(BrowserAccessibilityManager* manager,
                                ui::AXNode* node);
  ~BrowserAccessibilityAuraLinux() override;
  BrowserAccessibilityAuraLinux(const BrowserAccessibilityAuraLinux&) = delete;
  BrowserAccessibilityAuraLinux& operator=(
      const BrowserAccessibilityAuraLinux&) = delete;

  CONTENT_EXPORT ui::AXPlatformNodeAuraLinux* GetNode() const;

  // This is used to call UpdateHypertext, when a node needs to be
  // updated for some other reason other than via OnAtomicUpdateFinished.
  void UpdatePlatformAttributes() override;

  // BrowserAccessibility methods.
  void OnDataChanged() override;
  ui::AXPlatformNode* GetAXPlatformNode() const override;
  std::u16string GetHypertext() const override;

  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  ui::TextAttributeList ComputeTextAttributes() const override;

 private:
  // TODO: use a unique_ptr since the node is owned by this class.
  raw_ptr<ui::AXPlatformNodeAuraLinux> platform_node_;
};

CONTENT_EXPORT BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_
