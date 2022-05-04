// Copyright 2021 The Chromium Authors. All rights reserved.
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

  base::DictionaryValue dict;
  RecursiveBuildTree(*root_internal, &dict);
  return std::move(dict);
}

void AccessibilityTreeFormatterAndroidExternal::RecursiveBuildTree(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) const {
  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);

  // If a null string is returned, web contents likely doesn't exist, and it is
  // a sign that an accessibility service was disable. Print warning and escape.
  // TODO: It would be interesting to allow filtering here in the future.
  std::u16string str = android_node->GenerateAccessibilityNodeInfoString();
  if (str.empty()) {
    dict->SetStringKey(kStringKey, kErrorMessage);
    return;
  }

  dict->SetStringKey(kStringKey, str);

  base::Value::List children;

  for (size_t i = 0; i < node.PlatformChildCount(); ++i) {
    BrowserAccessibility* child_node = node.PlatformGetChild(i);
    std::unique_ptr<base::DictionaryValue> child_dict(
        new base::DictionaryValue);
    RecursiveBuildTree(*child_node, child_dict.get());
    children.Append(base::Value::FromUniquePtrValue(std::move(child_dict)));
  }
  dict->GetDict().Set(kChildrenDictAttr, std::move(children));
}

std::string AccessibilityTreeFormatterAndroidExternal::ProcessTreeForOutput(
    const base::DictionaryValue& dict) const {
  std::string line;
  if (dict.GetString(kStringKey, &line))
    return line;

  return std::string();
}

}  // namespace content
