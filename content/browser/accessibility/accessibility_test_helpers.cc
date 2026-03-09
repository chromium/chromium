// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_test_helpers.h"

#include "base/functional/function_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/platform/browser_accessibility.h"

namespace content {

namespace {

// DFS traversal returning the first node matching `predicate`.
ui::BrowserAccessibility* FindInSubtree(
    ui::BrowserAccessibility& node,
    base::FunctionRef<bool(ui::BrowserAccessibility&)> predicate) {
  if (predicate(node)) {
    return &node;
  }
  for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
    ui::BrowserAccessibility* result =
        FindInSubtree(*node.PlatformGetChild(i), predicate);
    if (result) {
      return result;
    }
  }
  return nullptr;
}

}  // namespace

ui::BrowserAccessibility* FindFirstAccessibilityNodeWithRoleAndNameOrValue(
    ui::BrowserAccessibility& root,
    ax::mojom::Role role,
    const std::string& name_or_value) {
  // Note that in the case of a text field,
  // "BrowserAccessibility::GetValueForControl" has the added functionality of
  // computing the value of an ARIA text box from its inner text.
  //
  // <div contenteditable="true" role="textbox">Hello world.</div>
  // Will expose no HTML value attribute, but some screen readers, such as Jaws,
  // VoiceOver and Talkback, require one to be computed.
  return FindInSubtree(root, [&](ui::BrowserAccessibility& node) {
    const std::string& name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    const std::string value = base::UTF16ToUTF8(node.GetValueForControl());
    return node.GetRole() == role &&
           (name == name_or_value || value == name_or_value);
  });
}

ui::BrowserAccessibility* FindFirstAccessibilityNodeWithRole(
    ui::BrowserAccessibility& root,
    ax::mojom::Role role) {
  return FindInSubtree(root, [&](ui::BrowserAccessibility& node) {
    return node.GetRole() == role;
  });
}

ui::BrowserAccessibility* FindFirstAccessibilityNodeWithNameOrValue(
    ui::BrowserAccessibility& root,
    const std::string& name_or_value) {
  return FindInSubtree(root, [&](ui::BrowserAccessibility& node) {
    const std::string& name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    const std::string value = base::UTF16ToUTF8(node.GetValueForControl());
    return name == name_or_value || value == name_or_value;
  });
}

ui::BrowserAccessibility* FindFirstAccessibilityNodeWithStringAttribute(
    ui::BrowserAccessibility& root,
    ax::mojom::StringAttribute attr,
    const std::string& value) {
  return FindInSubtree(root, [&](ui::BrowserAccessibility& node) {
    return node.GetStringAttribute(attr) == value;
  });
}

}  // namespace content
