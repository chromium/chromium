// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_auralinux.h"

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    BrowserAccessibility* obj) {
  return static_cast<BrowserAccessibilityAuraLinux*>(obj);
}

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityAuraLinux();
}

BrowserAccessibilityAuraLinux::BrowserAccessibilityAuraLinux() {
  node_ = static_cast<ui::AXPlatformNodeAuraLinux*>(
      ui::AXPlatformNode::Create(this));
}

BrowserAccessibilityAuraLinux::~BrowserAccessibilityAuraLinux() {
  DCHECK(node_);
  node_->Destroy();
}

ui::AXPlatformNodeAuraLinux* BrowserAccessibilityAuraLinux::GetNode() const {
  return node_;
}

gfx::NativeViewAccessible
BrowserAccessibilityAuraLinux::GetNativeViewAccessible() {
  DCHECK(node_);
  return node_->GetNativeViewAccessible();
}

void BrowserAccessibilityAuraLinux::UpdatePlatformAttributes() {
  GetNode()->UpdateHypertext();
}

void BrowserAccessibilityAuraLinux::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();
  DCHECK(node_);
  node_->EnsureAtkObjectIsValid();
}

ui::AXPlatformNode* BrowserAccessibilityAuraLinux::GetAXPlatformNode() const {
  return GetNode();
}

base::string16 BrowserAccessibilityAuraLinux::GetText() const {
  return GetHypertext();
}

base::string16 BrowserAccessibilityAuraLinux::GetHypertext() const {
  return GetNode()->AXPlatformNodeAuraLinux::GetHypertext();
}

ui::TextAttributeList BrowserAccessibilityAuraLinux::ComputeTextAttributes()
    const {
  return GetNode()->ComputeTextAttributes();
}

}  // namespace content
