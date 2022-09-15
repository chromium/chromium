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

base::Value AccessibilityTreeFormatterAndroidExternal::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  if (!root) {
    return base::Value(base::Value::Type::DICTIONARY);
  }

  BrowserAccessibility* root_internal =
      BrowserAccessibility::FromAXPlatformNodeDelegate(root);

  base::Value::Dict dict;
  RecursiveBuildTree(*root_internal, &dict);
  return base::Value(std::move(dict));
}

void AccessibilityTreeFormatterAndroidExternal::RecursiveBuildTree(
    const BrowserAccessibility& node,
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

  for (size_t i = 0; i < node.PlatformChildCount(); ++i) {
    BrowserAccessibility* child_node = node.PlatformGetChild(i);
    base::Value::Dict child_dict;
    RecursiveBuildTree(*child_node, &child_dict);
    children.Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(children));
}

std::string AccessibilityTreeFormatterAndroidExternal::ProcessTreeForOutput(
    const base::DictionaryValue& dict) const {
  std::string line;
  if (dict.GetString(kStringKey, &line))
    return line;

  return std::string();
}

}  // namespace content
