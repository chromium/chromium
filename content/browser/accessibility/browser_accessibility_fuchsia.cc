// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_fuchsia.h"

namespace content {

BrowserAccessibilityFuchsia::BrowserAccessibilityFuchsia(
    BrowserAccessibilityManager* manager,
    ui::AXNode* node)
    : BrowserAccessibility(manager, node) {}

// static
std::unique_ptr<BrowserAccessibility> BrowserAccessibility::Create(
    BrowserAccessibilityManager* manager,
    ui::AXNode* node) {
  return std::make_unique<BrowserAccessibilityFuchsia>(manager, node);
}

BrowserAccessibilityFuchsia::~BrowserAccessibilityFuchsia() {
  // TODO(fxb.dev/82820): Request deletion from accessibility bridge.
}

fuchsia::accessibility::semantics::Node
BrowserAccessibilityFuchsia::ToFuchsiaNode() const {
  // TODO(fxb.dev/79146): Implement.

  return fuchsia::accessibility::semantics::Node();
}

void BrowserAccessibilityFuchsia::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();

  // TODO(fxb.dev/82820): Add code path for a node to notify fuchsia that
  // its data has changed.
}

void BrowserAccessibilityFuchsia::OnLocationChanged() {
  // TODO(fxb.dev/82822): Add code path for a node to notify fuchsia that
  // its location has changed.
}

BrowserAccessibilityFuchsia* ToBrowserAccessibilityFuchsia(
    BrowserAccessibility* obj) {
  return static_cast<BrowserAccessibilityFuchsia*>(obj);
}

}  // namespace content
