// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_android_external.h"

#include <string>

#include "content/browser/accessibility/browser_accessibility_android.h"

static const char kStringKey[] = "key";
static const char kErrorMessage[] = "Error";

namespace content {

AccessibilityTreeFormatterAndroidExternal::
    AccessibilityTreeFormatterAndroidExternal() = default;

AccessibilityTreeFormatterAndroidExternal::
    ~AccessibilityTreeFormatterAndroidExternal() = default;

base::Value::Dict AccessibilityTreeFormatterAndroidExternal::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  if (!root) {
    return base::Value::Dict();
  }

  base::Value::Dict dict;
  RecursiveBuildTree(*root, &dict);
  return dict;
}

void AccessibilityTreeFormatterAndroidExternal::RecursiveBuildTree(
    const ui::AXPlatformNodeDelegate& node,
    base::Value::Dict* dict) const {
  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);

  // If a null string is returned, web contents likely doesn't exist, and it is
  // a sign that an accessibility service was disable. Print warning and escape.
  // TODO: It would be interesting to allow filtering here in the future.
  std::u16string str = android_node->GenerateAccessibilityNodeInfoString();
  if (str.empty()) {
    dict->Set(kStringKey, kErrorMessage);
    return;
  }

  dict->Set(kStringKey, str);

  base::Value::List children;

  for (size_t i = 0; i < node.GetChildCount(); ++i) {
    ui::BrowserAccessibility* child_node = android_node->PlatformGetChild(i);
    base::Value::Dict child_dict;
    RecursiveBuildTree(*child_node, &child_dict);
    children.Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(children));
}

std::string AccessibilityTreeFormatterAndroidExternal::ProcessTreeForOutput(
    const base::Value::Dict& dict) const {
  const std::string* line = dict.FindString(kStringKey);
  if (line) {
    return *line;
  }
  return std::string();
}

}  // namespace content
