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
std::unique_ptr<BrowserAccessibility> BrowserAccessibility::Create(
    BrowserAccessibilityManager* manager,
    ui::AXNode* node) {
  BrowserAccessibility* wrapper = manager->GetFromAXNode(node);
  bool is_focused_node = wrapper && wrapper == manager->GetLastFocusedNode();
  auto platform_node =
      std::make_unique<BrowserAccessibilityAuraLinux>(manager, node);
  if (is_focused_node)
    platform_node->GetNode()->SetAsCurrentlyFocusedNode();
  return platform_node;
}

BrowserAccessibilityAuraLinux::BrowserAccessibilityAuraLinux(
    BrowserAccessibilityManager* manager,
    ui::AXNode* node)
    : BrowserAccessibility(manager, node) {
  node_ = static_cast<ui::AXPlatformNodeAuraLinux*>(
      ui::AXPlatformNode::Create(this));
}

BrowserAccessibilityAuraLinux::~BrowserAccessibilityAuraLinux() {
  DCHECK(node_);
  node_->Destroy();
  node_ = nullptr;
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

std::u16string BrowserAccessibilityAuraLinux::GetHypertext() const {
  return GetNode()->AXPlatformNodeAuraLinux::GetHypertext();
}

ui::TextAttributeList BrowserAccessibilityAuraLinux::ComputeTextAttributes()
    const {
  return GetNode()->ComputeTextAttributes();
}

}  // namespace content
