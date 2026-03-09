// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TEST_HELPERS_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TEST_HELPERS_H_

#include <string>

#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {
class BrowserAccessibility;
}  // namespace ui

namespace content {

// Find the first node in DFS order matching the given role whose accessible
// name or value equals `name_or_value`.
ui::BrowserAccessibility* FindFirstAccessibilityNodeWithRoleAndNameOrValue(
    ui::BrowserAccessibility& root,
    ax::mojom::Role role,
    const std::string& name_or_value);

// Find the first node in DFS order matching the given role.
ui::BrowserAccessibility* FindFirstAccessibilityNodeWithRole(
    ui::BrowserAccessibility& root,
    ax::mojom::Role role);

// Find the first node in DFS order whose accessible name or value for control
// equals `name_or_value`.
ui::BrowserAccessibility* FindFirstAccessibilityNodeWithNameOrValue(
    ui::BrowserAccessibility& root,
    const std::string& name_or_value);

// Find the first node in DFS order whose value for the given string attribute
// equals `value`.
ui::BrowserAccessibility* FindFirstAccessibilityNodeWithStringAttribute(
    ui::BrowserAccessibility& root,
    ax::mojom::StringAttribute attr,
    const std::string& value);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TEST_HELPERS_H_
